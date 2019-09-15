for i in /dev/freezer/AFF_*; do echo THAWED > $i/freezer.state; for j in  `cat $i/tasks`; do kill -9 $j;   done; done;

rmdir /dev/cpuset/AFF_*
rmdir /dev/freezer/AFF_*
