ALIGN=4096

BIN=$1
shift
LIST=$1
shift

echo BUNDLING $@ INTO $BIN $LIST

echo -n > $LIST

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

  echo "{ \"$(basename $D)\", $ALIGNED },"  >> $LIST

  dd of=$BIN if=$B obs=$ALIGN seek=$(($P / $ALIGN)) || exit 1

  P=$(($P + $ALIGNED))
done

exit 0

