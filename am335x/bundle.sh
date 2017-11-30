
ALIGN=4096

BIN=$1
shift
HEAD=$1
shift

echo -n > $HEAD

P=0
for F in $@
do
	SIZE=$(du -b $F | \
	       awk "{print and((\$1 + $ALIGN - 1), compl($ALIGN-1))}") 
	echo "{ \"$F\", $SIZE }," >> $HEAD
	dd of=$BIN if=$F obs=$ALIGN seek=$(($P / $ALIGN))
	P=$(($P + $SIZE))
done

