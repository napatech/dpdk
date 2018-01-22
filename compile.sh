#!/usr/bin/env bash

rm -rf x86_64-native-linuxapp-gcc/
rm -rf x86_64-native-linuxapp-clang/


make config T=x86_64-native-linuxapp-gcc

echo "<<<<<<<<< x86_64-native-linuxapp-gcc - default >>>>>>>>>>>>>>>>>>>"

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - default"
  exit 1
fi

echo "<<<<<<< x86_64-native-linuxapp-gcc - Shared lib >>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(BUILD_SHARED_LIB=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - Shared lib"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-gcc - SW Stat >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - SW Stat"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-gcc - No Default filter >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - No Default filter"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-gcc - Copy offset >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - Copy offset"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-gcc - All >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-gcc - All"
  exit 1
fi







make config T=x86_64-native-linuxapp-clang

echo "<<<<<<<<< x86_64-native-linuxapp-clang - default >>>>>>>>>>>>>>>>>>>"

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - default"
  exit 1
fi

echo "<<<<<<< x86_64-native-linuxapp-clang - Shared lib >>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(BUILD_SHARED_LIB=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - Shared lib"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-clang - SW Stat >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - SW Stat"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-clang - No Default filter >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1n,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - No Default filter"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-clang - Copy offset >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1n,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - Copy offset"
  exit 1
fi

echo "<<<<<<<< x86_64-native-linuxapp-clang - All >>>>>>>>>>>>>>>>>>>>"
make clean -j

sed -ri 's,(PMD_NTACC_USE_SW_STAT=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_DISABLE_DEFAULT_FILTER=).*,\1y,' build/.config
sed -ri 's,(PMD_NTACC_COPY_OFFSET=).*,\1y,' build/.config

make -j

if [ ${?} -ne 0 ]; then
  echo "Failed to compile x86_64-native-linuxapp-clang - All"
  exit 1
fi

