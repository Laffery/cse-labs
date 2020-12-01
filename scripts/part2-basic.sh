echo "start 2PL test-lab3-part2-basic"
./start_ydb.sh OCC
rm tmp.t 2>/dev/null
timeout 30s ./test-lab3-part2-3-basic 4772 | tee tmp.t
cat tmp.t 2>/dev/null | grep -q '[^_^] Pass'
if [ $? -ne 0 ]; then
    	echo "Failed part2 basic"
else
	passedtest=$((passedtest+1))
     	echo "Passed part2 basic"
fi
rm tmp.t 2>/dev/null
./stop_ydb.sh