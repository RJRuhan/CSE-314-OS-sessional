#!/usr/bin/bash

usageMsg="Usage:\n./organize.sh <submission folder> <target folder> <test folder> <answer folder> [-v] [-noexecute]\n
-v: verbose\n-noexecute: do not execute code files\n"

goto(){
	if [ -d "$1" ]
	then
		cd "$1"
		return 1
	else
		return 0
	fi
}

visit()
{
	if [ -d "$1" ]
	then
	
		for i in "$1"/*
		do
			visit "$i"
            if [ $? = 1 ]
            then
                return 1
            fi
        done
	
	elif [ -f "$1" ]
	then
		if [[ "$1" = *.c ]] || [[ "$1" = *.py ]] || [[ "$1" = *.java ]]
        then
            echo "$1"
            return 1
        fi
	fi

    return 0
}

deleteExceptZipFiles()
{
	for i in "$1"/*
	do
		if [[ "$i" != *.zip ]]
		then
			rm -r "$i"
		fi
	done

}

if [ $# -lt 4 ] || [ $# -gt 6 ]
then
	echo -e "$usageMsg"
	exit
fi

for arg in ${@:1:4}
do
	if [ "${arg::1}" = "-" ]
	then
		echo -e "$usageMsg"
		exit
	fi
done

optioanlArgs=( "${@:5:6}" )
verbose="false"
noexecute="false"


for arg in ${optioanlArgs[@]}
do
	# echo $arg
	if [ $arg = "-v" ]
	then
		verbose="true"
	elif [ $arg = "-noexecute" ]
	then
		noexecute="true"
	else
		echo -e $usageMsg
		exit
	fi
done

submissionFolder=$1
targetsFolder=$2
testsFolder=$3
answersFolder=$4

goto "$answersFolder"
res=$?

if [ $res = 0 ]
then
	echo "$answersFolder: No such folder exists"
	exit
fi

cd ..
goto "$testsFolder"
res=$?

if [ $res = 0 ]
then
	echo "$testsFolder: No such folder exists"
	exit
fi

i=0
for test in *
do
	((i++))
done

if [ $verbose = true ]
then
	echo "Found $i test files"
fi

cd ..

goto "$submissionFolder"
res=$?

if [ $res = 0 ]
then
	echo "$submissionFolder: No such folder exists"
	exit
fi

declare -a students
declare -a ftypes
declare -a matchedArr
declare -a unmatchedArr



for file in *.zip
do
	# roll=$( echo $file | cut -d '_' -f 5 | cut -d '.' -f 1)
	roll=${file%.zip}
	roll=${roll: -7}

	# echo $roll
	if [ $verbose = true ]
	then
		echo "Organizing files of $roll"
	fi


	students+=("$roll")

	unzip -q "$file"

	fpath=$(visit .)

	if [ $? = 0 ]
	then
		echo "failed to find the main file..."
		deleteExceptZipFiles .
		continue
	fi

	# echo $fpath

	# ftype=$( echo "$fpath" | cut -d '.' -f 2 )
	ftype="garbage"

	folderName="garbage"

	if [[ $fpath = *.c ]]
	then
		ftype=c
		folderName=C
	elif [[ $fpath = *.py ]]
	then
		ftype=py
		folderName=Python
	elif [[ $fpath = *.java ]]
	then
		ftype=java
		folderName=Java	
	fi

	# echo $ftype
	# echo $folderName

	ftypes+=("$folderName")

	
	mkdir -p ../"$targetsFolder"/"$folderName"/"$roll"

	mainFilePath=""

	if [ $ftype = java ]
	then
		mainFilePath="$targetsFolder"/"$folderName"/"$roll"/Main."$ftype"
	else
		mainFilePath="$targetsFolder"/"$folderName"/"$roll"/main."$ftype"
	fi 

	execFilePath="garbage"
	# echo $mainFilePath

	cp "$fpath" ../"$mainFilePath"

	if [ $noexecute = "true" ]
	then
		deleteExceptZipFiles .
		continue
	fi

	cd ..

	if [ $verbose = true ]
	then
		echo "Executing files of $roll"
	fi


	if [ $ftype = c ]
	then
		execFilePath="$targetsFolder"/"$folderName"/"$roll"/main.out
		gcc ./"$mainFilePath" -o ./"$execFilePath"
	elif [ $ftype = java ]
	then
		execFilePath="$targetsFolder"/"$folderName"/"$roll"/
		javac ./"$mainFilePath"
	else
		execFilePath="$targetsFolder"/"$folderName"/"$roll"/main.py
	fi

	goto "$testsFolder"
	res=$?

	if [ $res = 0 ]
	then
		cd ./"$submissionFolder"
		echo "No such tests folder exist"
		deleteExceptZipFiles .
		continue
	fi


	count=1
	matched=0
	unmatched=0

	for test in *
	do
		# echo $test
		if [ $ftype = c ]
		then
			../"$execFilePath" < "$test" > ../"$targetsFolder"/"$folderName"/"$roll"/out"$count".txt
		elif [ $ftype = java ]
		then
			java -cp ../"$execFilePath" Main < "$test" > ../"$targetsFolder"/"$folderName"/"$roll"/out"$count".txt			
		else
			python3 ../"$execFilePath" < "$test" > ../"$targetsFolder"/"$folderName"/"$roll"/out"$count".txt
		fi

		ret=$(diff ../"$targetsFolder"/"$folderName"/"$roll"/out"$count".txt ../"$answersFolder"/ans"$count".txt -q)
		
		ret=$?
		if [ $ret = 1 ]
		then
			unmatched=$((unmatched + 1))
		elif [ $ret = 0 ]
		then
			matched=$((matched + 1))
		fi

		count=$(($count + 1))
	done

	# echo $unmatched
	# echo $matched

	matchedArr+=("$matched")
	unmatchedArr+=("$unmatched")

	goto ../"$submissionFolder"

	deleteExceptZipFiles .
	
	# exit
	
done


# echo "${students[@]}"
# echo "${ftypes[@]}"
# echo "${matchedArr[@]}"
# echo "${unmatchedArr[@]}"

cd ../"$targetsFolder"

if [ $noexecute = false ]
then
	touch result.csv
	echo "student_id,type,matched,not_matched" > result.csv

	i=0
	for roll in ${students[@]}
	do
		echo "$roll,${ftypes[$i]},${matchedArr[$i]},${unmatchedArr[$i]}" >> result.csv

		((i++))
	done
fi 
