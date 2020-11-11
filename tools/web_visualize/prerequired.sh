#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/neuware/lib64/

if grep -Eqi "Ubuntu" /etc/issue || grep -Eq "Ubuntu" /etc/*-release; then
  echo "Ubuntu system"
  apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends \
    python3-dev \
    python3-pip && \
    apt-get clean && \
    pip3 install --upgrade pip && \
    pip3 install -r requirements.txt
elif grep -Eqii "CentOS" /etc/issue || grep -Eq "CentOS" /etc/*-release; then
  echo "CentOS system"
  yum update -y && yum install -y python36 python36-libs python36-devel python36-pip && \
    yum clean all && \
    pip3 install --upgrade pip && \
    pip3 install -r requirements.txt
fi

