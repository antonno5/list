#!/bin/sh

exit_code=1
stage_log_file="last_stage_log"
log_file="last_log"
task="list"


echo "" > $log_file

AppendToLog() {
    echo "-------- $1 --------" >> $log_file
    cat $stage_log_file >> $log_file
    echo "" >> $log_file
}

TestStage () {
   cmd=$1
   success=$2
   fail=$3
   echo ""
   echo "executing \"$cmd\""
   start_time=`date +%s`
   if $cmd > $stage_log_file 2>&1; then
       echo finished in $(expr `date +%s` - $start_time) seconds
       AppendToLog "$cmd"
       echo $success
       exit_code=$((exit_code+1))
   else
       AppendToLog "(failed) $cmd"
       echo $fail
       echo "Printing last stage output. See \"$log_file\" for the full log"
       cat $stage_log_file
       exit $exit_code
   fi
}


TestStage "sha256sum -c checksums" "File integrity confirmed" "WARNING! some of the repository files were modified, test results will be ignored"

if g++ --version > /dev/null; then
    touch test.cpp
    TestStage "./build.sh -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc" "gcc build complete" "gcc build failed"

    TestStage "./build/$task" "Tests finished successfully!" "Tests failed"
else
    echo "Did not find gcc compiler. Test results may differ from final verdict"
fi

if clang++ --version > /dev/null; then
    touch test.cpp
    clang_cmd="./build.sh -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang"
    TestStage "$clang_cmd" "clang build complete" "clang build failed"

    TestStage "./build/$task" "Tests finished successfully!" "Tests failed"

    if [ ! -e compile_commands.json ]; then
        ln -s build/compile_commands.json compile_commands.json
    fi

    if clang-tidy --version > /dev/null; then
        TestStage "clang-tidy stackallocator.h" "Codestyle check successful!" "First codestyle check failed"
    else
        echo "Did not find clang-tidy. Test results may differ from final verdict"
    fi
    
    #echo "clang-format is temporarily disabled"
    #if clang-format --version > /dev/null; then
    #    # detect indentation width
    #    indent=4
    #    if cat $task.h | grep "^  String" > /dev/null; then
    #        indent=2
    #    fi
    #    echo "Assume indentation witdh = $indent" >> $log_file
    #    format_options=$(cat .clang-format)
    #    format_options="{${format_options}, IndentWidth: $indent}"
    #    clang-format -style="${format_options}" string.h > ._string.h
    #    TestStage "diff string.h ._string.h -B" "Second codestyle check succesful" "Second codestyle check failed"
    #    # TODO: ._string.h is not deleted on failed test
    #    rm ._string.h
    #else
    #    echo "Did not find clang-format. Test results may differ from final verdict"
    #fi
else
    echo "Did not find clang compiler. Test results may differ from final verdict"
fi

echo "All checks passed!"
