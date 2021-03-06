---
layout: documentation
title: Documentation - Tutorial 1
---

# Tutorial 1: Compilation and Installation

## Index

1. [Introduction](#introduction)
2. [Requirements](#requirements)
3. [Compilation](#compilation)
4. [Installation](#installation)

## Introduction

In this tutorial, Jool will be compiled and installed from scratch.

Jool is two things:

1. A <a href="https://en.wikipedia.org/wiki/Loadable_kernel_module" target="_blank">kernel module</a> you can hook up to the Linux kernel. This is the main IPv6-IPv4 translating component and all you need for basic use.
2. A <a href="https://en.wikipedia.org/wiki/User_space" target="_blank">userspace</a> application which can be used to configure the module.

When you put it that way, there is really nothing unusual about Jool's installation. But I figure some of our users might have no previous experience meddling with drivers, so this first tutorial will serve as an introduction to at least give them an idea of what each step does.

This page will only meedle with the module; the userspace application is not needed until later, so it's covered in its own [document](userspace-app.html).

If you came here already installed, maybe you want to go straight to [the second tutorial](tutorial2.html) for an insight on deployment.

## Requirements

First off, the computer that will be translating traffic needs a kernel (again, Linux) whose version is 3.0.0 to 3.11.5. Higher versions are probably fine, but we haven't tested them thoroughly. We do not recommend using Linux 3.12 because of the reasons outlined <a href="https://github.com/NICMx/NAT64/issues/90" target="_blank">here</a>.

Use the `uname -r` command to know your kernel version.

{% highlight bash %}
$ /bin/uname -r
3.5.0-45-generic
$ # OK, fine.
{% endhighlight %}

If you're just getting acquainted with NAT64, some people have an easier time picturing the ordeal when the translator has two separate network interfaces (one to interact with IPv6 networks, one for IPv4 networks). But this is not a requirement; since dual stacking exists, you can get away with only one.

Because the tutorials are first and foremost a tool to get newcomers on the right mindset, most of the deployment discussion will assume two separate interfaces (exemplified below: eth0 and eth1).

{% highlight bash %}
$ /sbin/ip link show
(...)
2: eth0: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN qlen 1000
    link/ether 08:00:27:3d:24:77 brd ff:ff:ff:ff:ff:ff
3: eth1: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN qlen 1000
    link/ether 08:00:27:ca:18:c8 brd ff:ff:ff:ff:ff:ff
{% endhighlight %}

## Compilation

Each kernel version combined with each different architecture requires different binaries, so providing packages for every combination would be impossibly cumbersome. For this reason, what you'll download from either the official site or the Github page is the source; there is no way around compiling the code yourself.

On the flip side, kernel modules cannot (AFAIK) have dependencies other than your kernel headers and a good compiler, so the procedure is fairly painless.

> By the way, Git users: We no longer use the code's master branch for potentially destructive development. <a href="https://github.com/NICMx/NAT64" target="_blank">github.com/NICMx/NAT64</a> should _always_ contain the latest stable version. Even if the git repository has tags, you should stick to the latest commit of the master branch if you want the latest bugfixes.
> 
> Also, note that you will need <a href="https://www.gnu.org/software/autoconf/" target="_blank">autoconf</a> and <a href="https://www.gnu.org/software/automake/" target="_blank">automake</a> if you want to compile the userspace application from the git source.

You need:

* Jool. As stated above, you can download the git source, but using the [official release](download.html) might prove less convoluted.
* unzip (just to extract Jool).
	- Most distros feature it by default.
* gcc and make. This is distro-specific. Here's a few pointers, depending on your packaging tool:
	- Apt-get (Debian): `# apt-get install build-essential`
	- Pacman (Arch Linux): `# pacman -S base-devel`
	- Zypper (OpenSUSE): `# zypper install gcc make`
	- If you're using Ubuntu or CentOS, you should already have them.
* Your kernel headers (Any kernel module depends on these):
	- Apt-get (Debian, Ubuntu): `# apt-get install linux-headers-$(uname -r)`
	- Pacman (Arch Linux): `# pacman -S base-devel` (Right, you don't have to do it again.)
	- If you're using CentOS: `# apt-get install linux-headers-$(uname -r)`
	- Zypper (OpenSUSE): `# zypper install kernel-source`
* (And just to make sure Jool works as expected) ethtool
	- Apt-get: `# apt-get install ethtool`
	- Pacman: `# pacman -S ethtool`
	- Zypper: `# zypper install ethtool`

You might be used to a cute standard three-step procedure to compile and install programs: `./configure && make && make install`. Kernel modules do not follow it, but have a special one on their own.

As far as the compilation goes, there is no `configure` script. But you also don't have to edit the Makefile; you jump straight to `make` and you're done. The Makefile can be found in the `mod` folder:

{% highlight bash %}
user@node:~$ unzip Jool-<version>.zip
user@node:~$ cd Jool-<version>/mod
user@node:~/Jool-<version>/mod$ make
{% endhighlight %}

And that's that.

## Installation

You copy the binaries generated to your system's module pool by running the `modules_install` target:

{% highlight bash %}
user@node:~/Jool-<version>/mod# make modules_install
{% endhighlight %}

You'll later activate the module using the `modprobe` command. Thing is, the fact that the module resides in your pool doesn't mean it has already been indexed. Use `depmod` to make `modprobe` aware of the new module:

{% highlight bash %}
user@node:~# /sbin/depmod
{% endhighlight %}

Done; Jool can now be started. In order to run it, head to the [second tutorial](tutorial2.html).

