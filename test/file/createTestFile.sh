#! /bin/sh
indexfilename="0.log"
content=""
i=1;
max=$1;
while [ $i -le $max ];
do
name=`printf "%d.log" $i`;
`echo $i >> $name`;
`echo "sdfsdfsdf" >> $name`;
temp="$name"" ";
content=$content$temp;
i=$(($i+1));
done

`echo $content > $indexfilename`


