arr=("0.0" "0.1" "0.2" "0.3" "0.4" "0.5" "0.6" "0.7" "0.8" "0.9" "1.0")

echo "" > ./exp_results/${1}.out

for ratio in ${arr[@]}; do
    echo "${ratio}" >> ./exp_results/${1}.out
    echo ""  >> ./exp_results/${1}.out
    python3 ./agg.py ./exp_results/${1}-$ratio.out >> ./exp_results/${1}.out
done 