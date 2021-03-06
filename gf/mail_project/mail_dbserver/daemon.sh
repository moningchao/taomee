#!/bin/sh
echo "ser shell exec ..."
if [ "$1" = "stop" ] ; then
#	ps -ef |grep ./ser | awk '{print "kill -15 " $2}'|sh
	ps -ef |grep "\<gfnowser\>" | awk '{print "kill -9 " $2}'|sh
elif [ "$1" = "restart" ]; then
	killall -HUP ./gfnowser
elif [ "$1" = "reboot" ]; then
	ps -ef |grep "\<gfnowser\>" | awk '{print "kill -9 " $2}'|sh
	./gfnowser ./bench.conf ./libgfser.so 

elif [ "$1" = "start" ]; then
#	 valgrind --leak-check=full ./gfnowser ../etc/bench.conf ./libtest.so -s 4096
	./gfnowser ./bench.conf ./libgfser.so 
elif [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
	echo "daemon.sh start|stop|restart"
fi
#ls ./gfser_plus ../etc/bench.conf ./libgfser.so
