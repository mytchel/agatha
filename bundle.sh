
ALIGN=4096

BIN=$1
shift
LIST=$1
shift

echo -n > $LIST

P=0
for D in $@
do
  B=$D/$(basename $D).bin
  E=$D/$(basename $D).elf

  SIZE=$(ls -l $B | cut -d ' ' -f 8)
  test $? || exit 1

  ALIGNED=$(echo $SIZE | \
    awk "{print and((\$1 + $ALIGN - 1), compl($ALIGN-1))}")

  echo "{ \"$(basename $D)\", $ALIGNED },"  >> $LIST

  dd of=$BIN if=$B obs=$ALIGN seek=$(($P / $ALIGN)) || exit

  P=$(($P + $ALIGNED))
done

exit 0

