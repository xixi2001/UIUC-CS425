./client < test.input

diff grep_result.log test.result > diff_file
FN=$(awk 'END{print NR}' diff_file)

if [ $FN == '0' ]
then
    echo "Pass Unit Test"
else
    echo "Fail Unit test"
fi
