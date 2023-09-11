grep GET vm1.log > grep_tmp_1.log
grep GET vm2.log > grep_tmp_2.log
grep GET vm3.log > grep_tmp_3.log
grep GET vm4.log > grep_tmp_4.log
grep GET vm5.log > grep_tmp_5.log
grep GET vm6.log > grep_tmp_6.log
grep GET vm7.log > grep_tmp_7.log
grep GET vm8.log > grep_tmp_8.log
grep GET vm9.log > grep_tmp_9.log
grep GET vm10.log > grep_tmp_10.log

cat grep_tmp_1.log grep_tmp_2.log grep_tmp_3.log grep_tmp_4.log grep_tmp_5.log grep_tmp_6.log grep_tmp_7.log grep_tmp_8.log grep_tmp_9.log grep_tmp_10.log > vm_GET.log

./client < vm_GET.input
diff grep_result.log vm_GET.log > diff_file
FN=$(awk 'END{print NR}' diff_file)

if [ $FN == '0' ]
then
    echo "Pass Unit Test"
else
    echo "Fail Unit test"
fi
