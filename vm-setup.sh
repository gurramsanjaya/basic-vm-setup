#!/bin/bash
set -eo pipefail

: ${PORT:=59995}
: ${PLAN:=outplan}
: ${TLS_DIR:=cloudinit/tls}
: ${CLIENT_DIR:=wg_clients}
: ${TERRAFORM_CLIENT:=tofu}
: ${SKIP_CREATE:=false}
: ${SKIP_CREATE_WORKSPACE:=false}
: ${SKIP_HOSTS_MOD:=false}
: ${SKIP_WGE_CLIENT:=false}
: ${PRE_SLEEP_WGE_CLIENT:=150}
# set DN_OVERRIDE to override the DN value that will by default pull from openssl.cnf

## really unnecessary, but whatever
function banner() {
  echo '''
                                       __
 _   __ ____ ___          _____ ___   / /_ __  __ ____
| | / // __ `__ \ ______ / ___// _ \ / __// / / // __ \
| |/ // / / / / //_____/(__  )/  __// /_ / /_/ // /_/ /
|___//_/ /_/ /_/       /____/ \___/ \__/ \__,_// .___/
                                              /_/

'''
}

function hrule() {
  printf '\n%*s\n' "$(tput cols)" | sed 's/ /\xe2\x80\x95/g'
  if [ ! -z ${1+x} ]; then
    echo -e "$(tput bold)${1}: \n$(tput sgr0)"
  fi
}

# $1 - TLS_DIR, $2 - CA cert, $3 - cert
function check_cert() {
  echo -e "\nChecking cert - $2, with CA - $3:"
  local cacert=$(readlink -e "$1/$2")
  local cert=$(readlink -e "$1/$3")
  openssl verify -CAfile "$cacert" "$cert" &&
  openssl x509 -checkend 1000 -in "$cert"
}

function check_continue() {
  local continue
  read -p "Continue (y/n)? " continue
  if ! [[ $continue =~ ^[[:space:]]*[yY] ]]; then
    echo "quiting..."
    exit 0
  fi
}

function check_path() {
  local path
  if ! path=$(readlink -e "$1"); then
    echo "the following path doesn't exist: $1" >&2
    exit 1;
  fi
  echo "$path"
}

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

banner

if [ -z ${DN_OVERRIDE+x} ]; then
  DN=$(awk "$AWK_PRG" openssl.cnf)
else
  DN="$DN_OVERRIDE"
fi
echo "using domain name: ${DN}"


# basic checks
hrule "Basic Checks"
openssl --version
wge-client -version
jq --version
"$TERRAFORM_CLIENT" -version
if ! [[ $PRE_SLEEP_WGE_CLIENT =~ ^[0-9]+[smhd]?$ ]]; then
  echo -e "\ninvalid sleep time..."
  exit 1
fi
TLS_DIR=$(check_path "$TLS_DIR")
check_path "${TLS_DIR}/client.key"
check_path "${TLS_DIR}/client.pem"
CLIENT_DIR=$(check_path "$CLIENT_DIR")

# check certs first, disable fail on non-zero return status
hrule "Check Certs"
set +e
if ! (check_cert "$TLS_DIR" "rootCA.pem" "42_server.pem" && check_cert "$TLS_DIR" "rootCA.pem" "client.pem"); then
  echo -e "\ngenerate new cert chain"
  check_continue
  make clean-tls all-tls
  # check again
  if ! (check_cert "$TLS_DIR" "rootCA.pem" "42_server.pem" && check_cert "$TLS_DIR" "rootCA.pem" "client.pem"); then
    echo "\nsomething is wrong with cert generation... quiting..."
    exit 1
  fi
fi

set -e

mkdir -p "$CLIENT_DIR"

pushd vm 1>/dev/null
if [[ $SKIP_CREATE != "true" ]]; then
  if [[ $SKIP_CREATE_WORKSPACE != "true" ]]; then
    hrule "Workspace Create"
    read -p "Name of new Workspace: " NEW_WRKSP
    CUR_WRKSP=$("$TERRAFORM_CLIENT" workspace show) ## should we delete this?
    "$TERRAFORM_CLIENT" workspace new "$NEW_WRKSP"
  fi

  hrule "Instance Create"
  "$TERRAFORM_CLIENT" plan -out ${PLAN}
  "$TERRAFORM_CLIENT" apply ${PLAN}
fi


if [[ $SKIP_WGE_CLIENT == "true" ]]; then
  popd 1>/dev/null
else
  if [[ $SKIP_HOSTS_MOD != "true" ]]; then
    hrule "/etc/hosts Update"
    INSTANCE_IP=$("$TERRAFORM_CLIENT" output -json | jq -r .ipv4_address.value)
    echo "using ip retrieved from terraform state: ${INSTANCE_IP}"
    popd 1>/dev/null

    ## careful here, we are messing around with the /etc/hosts
    echo "removing domain for ${INSTANCE_IP} if any, removing ip for ${DN} if any, creating a new 1-1 entry with ${INSTANCE_IP} - ${DN}"
    check_continue

    sudo -s -- <<EOF
cp /etc/hosts /etc/hosts_old
sed -i '/.*${INSTANCE_IP}.*/d' /etc/hosts
sed -i '/.*${DN}.*/d' /etc/hosts
sed -i '$ {/^$/d}' /etc/hosts
printf '\n%s\t%s' $INSTANCE_IP "$DN" >> /etc/hosts
EOF
    sleep 2s
  fi

  hrule "Nslookup Verification"
  nslookup "$DN"
  read -p "press enter to continue..."

  hrule "Starting WG Exchange client"
  CWD=$(pwd)
  pushd "$CLIENT_DIR" 1>/dev/null
  if [[ $SKIP_CREATE != "true" ]]; then
    # sleep for atleast 2 1/2 minutes, cloud init takes time...
    echo "sleeping for ${PRE_SLEEP_WGE_CLIENT}, please wait..."
    sleep $PRE_SLEEP_WGE_CLIENT
  fi
  wge-client -key "${TLS_DIR}/client.key" -cert "${TLS_DIR}/client.pem" -conf "${CWD}/client.toml" -endpoint "https://${DN}:${PORT}"
  popd 1>/dev/null
fi
