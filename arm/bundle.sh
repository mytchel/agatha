ALIGN=4096

BIN=$1
shift
C=$1
shift

echo BUNDLING $@ INTO $BIN $C

echo -n > $C
echo "#include <types.h>" >> $C
echo "#include \"../bundle.h\"" >> $C
echo "struct bundle_proc bundled_procs[] = {" >> $C

P=0
for D in $@
do
  B=$D/$(basename $D).bin
  E=$D/$(basename $D).elf
	
	make -C $D $E $B || exit 1

  SIZE=$(ls -l $B | cut -d ' ' -f 8)
  test $? || exit 1

  ALIGNED=$(echo $SIZE | \
    awk "{print and((\$1 + $ALIGN - 1), compl($ALIGN-1))}")

  echo "{ \"$D\", $ALIGNED },"  >> $C

  dd of=$BIN if=$B obs=$ALIGN seek=$(($P / $ALIGN)) || exit 1

  P=$(($P + $ALIGNED))
done

echo "};" >> $C
echo "size_t nbundled_procs = sizeof(bundled_procs)/sizeof(bundled_procs[0]);" >> $C

exit 0
