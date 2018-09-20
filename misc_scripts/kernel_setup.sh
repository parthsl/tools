##### Install packages
###### `sudo dnf install bison automake autoconf flex ncurses-devel elfutils-libelf-devel openssl-devel`

echo "Enter 1 for Fedora-28 and 2 for Ubuntu"

if [ $1 = 1 ]; then
	sudo dnf install bison automake autoconf flex ncurses-devel elfutils-libelf-devel make gcc elfutils-devel libunwind-devel xz-devel numactl-devel openssl-devel slang-devel gtk2-devel perl-ExtUtils-Embed python-devel binutils-devel audit-libs-devel;
else
	if [ $1 = 2 ];then
		sudo apt install flex libncurses5 elfutils openssl libssl-dev automake bison ipmitool pkg-config libncurses5-dev
	fi
fi
