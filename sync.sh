if [ $# -eq 0 ]; then
	echo "Usage: `basename "$0"` <address> <port>"
else
	if [ -z "$2" ]; then
		port=22
	else
		port="$2"
	fi
	rsync --exclude='.*' --exclude=sync.sh -ravh -e "ssh -p $port" ./ fagelmatare-server@$1:Fagelmatare/modules/core/
fi