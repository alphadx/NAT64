/**
 * @file
 *
 * This module implements the feature mentioned in the RFC6146,
 * about managing static routes. It allows to add a new entry in
 * the BIB and Session tables from Userspace.
 */

#include "nat64/mod/static_routes.h"
#include "nat64/mod/config.h"
#include "nat64/mod/pool4.h"
#include "nat64/mod/bib.h"
#include "nat64/mod/session.h"
#include <linux/slab.h>


int add_static_route(struct request_bib *req)
{
	struct bib_entry *bib_by_ipv6, *bib_by_ipv4;
	struct bib_entry *bib = NULL;
	int error;

	if (!pool4_contains(&req->add.ipv4.address)) {
		log_err(ERR_POOL6_NOT_FOUND, "The address '%pI4' does not belong to the IPv4 pool.",
				&req->add.ipv4.address);
		return -EINVAL;
	}

	spin_lock_bh(&bib_session_lock);

	/* Check if the BIB entry exists. */
	error = bib_get_by_ipv6(&req->add.ipv6, req->l4_proto, &bib_by_ipv6);
	if (!error) {
		bib = bib_by_ipv6;
		goto already_mapped;
	}
	if (error != -ENOENT)
		goto generic_error;

	error = bib_get_by_ipv4(&req->add.ipv4, req->l4_proto, &bib_by_ipv4);
	if (!error) {
		bib = bib_by_ipv4;
		goto already_mapped;
	}
	if (error != -ENOENT)
		goto generic_error;

	/* Borrow the address and port from the IPv4 pool. */
	if (is_error(pool4_get(req->l4_proto, &req->add.ipv4))) {
		/*
		 * This might happen if Filtering just reserved the address#port, but hasn't yet inserted
		 * the BIB entry to the table. This is because bib_session_lock doesn't cover the IPv4
		 * pool.
		 * Otherwise something's not returning borrowed address#ports to the pool, which is an
		 * error.
		 */
		log_err(ERR_BIB_REINSERT, "Port number %u from address %pI4 is taken from the IPv4 pool, "
				"but it wasn't found in the BIB. Please try again; if the problem persists, "
				"please report.", req->add.ipv4.l4_id, &req->add.ipv4.address);
		error = -EEXIST;
		goto failure;
	}

	/* Create and insert the entry. */
	bib = bib_create(&req->add.ipv4, &req->add.ipv6, true);
	if (!bib) {
		log_err(ERR_ALLOC_FAILED, "Could NOT allocate a BIB entry.");
		error = -ENOMEM;
		goto failure;
	}

	error = bib_add(bib, req->l4_proto);
	if (error) {
		log_err(ERR_UNKNOWN_ERROR, "Could NOT add the BIB entry to the table.");
		goto failure;
	}

	spin_unlock_bh(&bib_session_lock);
	return 0;

already_mapped:
	log_err(ERR_BIB_REINSERT, "%pI6c#%u is already mapped to %pI4#%u.",
			&bib->ipv6.address, bib->ipv6.l4_id,
			&bib->ipv4.address, bib->ipv4.l4_id);
	error = -EEXIST;
	bib = NULL;
	goto failure;

generic_error:
	log_err(ERR_UNKNOWN_ERROR, "Error code %u while trying to interact with the BIB.",
			error);
	/* Fall through. */

failure:
	if (bib)
		bib_kfree(bib);
	spin_unlock_bh(&bib_session_lock);
	return error;
}

int delete_static_route(struct request_bib *req)
{
	struct bib_entry *bib;
	struct session_entry *session;
	int error = 0;

	spin_lock_bh(&bib_session_lock);

	switch (req->remove.l3_proto) {
	case L3PROTO_IPV6:
		error = bib_get_by_ipv6(&req->remove.ipv6, req->l4_proto, &bib);
		if (error)
			goto end;
		break;
	case L3PROTO_IPV4:
		error = bib_get_by_ipv4(&req->remove.ipv4, req->l4_proto, &bib);
		if (error)
			goto end;
		break;
	default:
		log_err(ERR_L3PROTO, "Unsupported network protocol: %u.", req->remove.l3_proto);
		error = -EINVAL;
		goto end;
	}

	if (!bib) {
		log_err(ERR_BIB_NOT_FOUND, "Could not find the BIB entry requested by the user.");
		error = -ENOENT;
		goto end;
	}

	/*
	 * I'm tempted to assert that the entry is static here. Would that serve a purpose?
	 * Nah.
	 */

	while (!list_empty(&bib->sessions)) {
		session = container_of(bib->sessions.next, struct session_entry, bib_list_hook);
		error = session_remove(session);
		if (error) {
			log_err(ERR_UNKNOWN_ERROR,
					"Session [%pI6c#%u, %pI6c#%u, %pI4#%u, %pI4#%u] refused to die.",
					&session->ipv6.remote.address, session->ipv6.remote.l4_id,
					&session->ipv6.local.address, session->ipv6.local.l4_id,
					&session->ipv4.local.address, session->ipv4.local.l4_id,
					&session->ipv4.remote.address, session->ipv4.remote.l4_id);
			goto end;
		}
		list_del(&session->bib_list_hook);
		list_del(&session->expire_list_hook);
		session_kfree(session);
	}

	error = bib_remove(bib, req->l4_proto);
	if (error) {
		log_err(ERR_UNKNOWN_ERROR, "Remove bib entry call ended with error code %d, "
				"despite validations.", error);
		goto end;
	}

	pool4_return(req->l4_proto, &bib->ipv4);
	bib_kfree(bib);
	/* Fall through. */

end:
	spin_unlock_bh(&bib_session_lock);
	return error;
}
