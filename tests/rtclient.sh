config_daemon -c ../configuration/example.xml &
registry_daemon &
register r 124 192.168.0.124 57844
./rtserver 123.xml 124