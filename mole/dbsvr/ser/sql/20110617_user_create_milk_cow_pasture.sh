#!/bin/bash
user="root"
password="ta0mee"
host="localhost"
tmp_file="table.sql"
create_user_milk_cow_pasture_table_sql() {
cat <<EOF >$tmp_file
CREATE TABLE IF NOT EXISTS t_user_milk_cow_pasture_$1(
	userid                   INT UNSIGNED NOT NULL DEFAULT 0,
	background      		 INT UNSIGNED NOT NULL DEFAULT 0,
	milk_storage			 INT UNSIGNED NOT NULL DEFAULT 0,
	day_count                INT UNSIGNED NOT NULL DEFAULT 0,
	grass_list				 varbinary(804) NOT NULL DEFAULT 0x00000000,
	store_date				 INT UNSIGNED NOT NULL DEFAULT 0,
	primary key (userid)
	)ENGINE=innodb,CHARSET=utf8;
EOF
}

db_index=0
end_index=100

while [ $db_index -lt $end_index ] ; do
	dbx=`printf "%02d" $db_index`
	echo $dbx
	table_index=0
	while [ $table_index -lt 10 ] ; do
		tbx=`printf "%d" $table_index`
		create_user_milk_cow_pasture_table_sql $tbx 
		cat $tmp_file | mysql -u $user --password="$password" -h $host "USER_$dbx" 
		let "table_index+=1"
	done
	let "db_index+=1"
done

