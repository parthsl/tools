tmp=$1;
bench="turbobench";
if [ $tmp ]; then
	bench=$tmp;
fi
until [ `pidof $bench` ]; do :; done;
ps -eLf | grep "$bench -h" | grep -v "grep" | cut -d" " -f9 | while read i; do ./setattr -p $i -j; done
