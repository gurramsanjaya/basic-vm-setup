#!/bin/bash
set -e

: ${PORT:=59995}
: ${PLAN:=outplan}
: ${CLIENT_DIR=wg_clients}
: ${TERRAFORM_CLIENT:=tofu}
: ${SKIP_CREATE:=true}

AWK_PRG='
{
  if(i) {
    second = match($0, /subjectAltName[[:space:]]*=[[:space:]]*DNS:([a-zA-Z0-9\.]*)[[:space:]]*/, arr);
    if(second != 0)
      print arr[1]
  } else {
    first = match($0, /\[server_extensions\]/);
    if(first != 0)
      i = 1
  }
}
'

DN=$(awk "$AWK_PRG" openssl.cnf)
echo "using domain name: ${DN}"


# basic checks
echo -e "\n=======Basic Checks=======\n"
wge-client -version
jq --version
${TERRAFORM_CLIENT} -version

mkdir -p "$CLIENT_DIR"

pushd vm
echo -e "\n=======Instance Create=====\n"
if [[ $SKIP_CREATE != "true" ]]; then
  ${TERRAFORM_CLIENT} plan -out ${PLAN}
  ${TERRAFORM_CLIENT} apply ${PLAN}
fi

echo -e "\n======/etc/hosts Update====\n"
INSTANCE_IP=$(${TERRAFORM_CLIENT} output -json | jq -r .ipv4_address.value)
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

