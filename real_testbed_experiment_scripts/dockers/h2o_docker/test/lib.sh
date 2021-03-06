#!/bin/bash

set -e

function finish {
	local exitCode=$?

	echo ".. cleanup"
        docker image rm -f test-buddy >/dev/null
        docker kill $(cat .cid) > /dev/null
        rm -f .cid

	echo "---------"
	if [ "$exitCode" == "0" ]; then
		echo "Test: SUCCESS"
	else
		docker logs $cid
		echo "Test: failed"
		exit $exitCode
	fi
}
trap finish EXIT


if [[ -z "${SKIP_BASEIMAGE_BUILD}" ]]; then
  # build image
  cd ../..
  echo -n "building base image ... "
  docker build -t test-h2o . > /dev/null
  echo ok
  cd $OLDPWD
fi

docker build -t test-buddy -f Dockerfile.test . >/dev/null

# cleanup, in case of previous error
rm -f .cid