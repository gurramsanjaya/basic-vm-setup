#!/bin/bash
set -e

: ${PORT:=59995}
: ${PLAN:=outplan}
: ${CLIENT_DIR=wg_clients}
: ${TERRAFORM_CLIENT:=tofu}
: ${SKIP_CREATE:=true}
: ${SKIP_WGE_CLIENT:=false}


# this tries matching '[server_extensions]' and then looks for the first 'subjectAltName = DNS:<domain>' along the next lines
AWK_PRG='
BEGIN { i=0 }
{
  if(i) {
    second = match($0, /subjectAltName[[:space:]]*=[[:space:]]*DNS:([a-zA-Z0-9\.]*)[[:space:]]*/, arr);
    if(second) {
      print arr[1]
      exit 0
    }
  } else {
    first = match($0, /\[server_extensions\]/);
    if(first)
      i = 1
  }
}
'
if [ -z ${DN_OVERRIDE+x} ]; then
  DN=$(awk "$AWK_PRG" openssl.cnf)
else
  DN="$DN_OVERRIDE"
fi
echo "using domain name: ${DN}"


# basic checks
echo -e "\n=======Basic Checks=======\n"
wge-client -version
jq --version
"$TERRAFORM_CLIENT" -version

mkdir -p "$CLIENT_DIR"

pushd vm
if [[ $SKIP_CREATE != "true" ]]; then
  echo -e "\n=======Instance Create=====\n"
  "$TERRAFORM_CLIENT" plan -out ${PLAN}
  "$TERRAFORM_CLIENT" apply ${PLAN}
fi

if [[ $SKIP_WGE_CLIENT != "true" ]]; then
echo -e "\n======/etc/hosts Update====\n"
  INSTANCE_IP=$("$TERRAFORM_CLIENT" output -json | jq -r .ipv4_address.value)
  echo "using ip retrieved from state: ${INSTANCE_IP}"
  popd

  ## careful here, we are messing around with the /etc/hosts
  echo "replacing domain for ${INSTANCE_IP} if any and saving it with ${DN}"
  sudo -s -- <<EOF
cp /etc/hosts /etc/hosts_old
sed -i '/.*${INSTANCE_IP}.*/d' /etc/hosts
sed -i '$ {/^$/d}' /etc/hosts
printf '\n%s\t%s' $INSTANCE_IP "$DN" >> /etc/hosts
EOF

  echo -e "\n===Nslookup Verification===\n"
  nslookup "$DN"
  read -p "press enter to continue..."

  echo -e "\n======Starting client======\n"
  CWD=$(pwd)
  pushd "$CLIENT_DIR"
  if [[ $SKIP_CREATE != "true" ]]; then
    # sleep for atleast 1 minute, cloud init takes time
    sleep 70
  fi
  wge-client -key "${CWD}/tls/client.key" -cert "${CWD}/tls/client.pem" -conf "${CWD}/client.toml" -endpoint "https://${DN}:${PORT}"
  popd
fi
