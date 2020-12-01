echo "start 2PL test-lab3-part3-b"
./start_ydb.sh 2PL
rm tmp.t 2>/dev/null
timeout 30s ./test-lab3-part3-b 4772 | tee tmp.t
cat tmp.t 2>/dev/null | grep -q '[^_^] Pass'
if [ $? -ne 0 ]; then
    	echo "Failed part2 special b"
else
	passedtest=$((passedtest+1))
       	echo "Passed part2 special b"
fi
rm tmp.t 2>/dev/null
./stop_ydb.sh
