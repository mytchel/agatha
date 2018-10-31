ALIGN=4096

NAME=$1
shift
BIN=$1
shift
C=$1
shift
P=$1
shift

echo >> $C
echo "struct bundle_proc $NAME[] = {" >> $C

for D in $@
do
  B=$D/out.bin
  E=$D/out.elf
	
  SIZE=$(ls -l $B | cut -d ' ' -f 8)
  test $? || exit 1

  ALIGNED=$(echo $SIZE | \
    awk "{print and((\$1 + $ALIGN - 1), compl($ALIGN-1))}")

  echo "{ \"$D\", $ALIGNED },"  >> $C

  dd of=$BIN if=$B obs=$ALIGN seek=$(($P / $ALIGN)) 2> /dev/null || exit 1

  P=$(($P + $ALIGNED))
done

echo "};" >> $C
echo >> $C
echo "size_t n$NAME = sizeof($NAME)/sizeof($NAME[0]);" >> $C
echo >> $C

echo $P

exit 0

