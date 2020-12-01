echo "start 2PL test-lab3-part2-a"
./start_ydb.sh 2PL
rm tmp.t 2>/dev/null
timeout 3s ./test-lab3-part2-a 4772 | tee tmp.t
cat tmp.t 2>/dev/null | grep -q '[^_^] Pass'
if [ $? -ne 0 ]; then
   	echo "Failed part2 special a"
else
  	echo "Passed part2 special a"
fi
rm tmp.t 2>/dev/null
./stop_ydb.sh
