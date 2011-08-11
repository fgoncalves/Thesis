config_daemon -c ../configuration/example.xml &
registry_daemon &
register r 123 192.168.0.123 57843
./rtserver 124.xml