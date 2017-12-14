source $PWD/dpdk.env
make EXTRA_CFLAGS="-g"  config T=x86_64-native-linuxapp-gcc
make EXTRA_CFLAGS="-g"  install T=x86_64-native-linuxapp-gcc -j28 $@
make EXTRA_CFLAGS="-g" -C examples -j28
