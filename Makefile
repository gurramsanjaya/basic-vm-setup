## tls stuff here
# modify these subjectNames as needed
ROOT_SUBJ ?= "/C=JP/O=Stardust Crusaders/CN=Root CA"
CLIENT_SUBJ ?= "/C=JP/O=Diamond Is Unbreakable/CN=WG-Client"
SERVER_SUBJ ?= "/C=JP/O=Steel Ball Run/CN=WG-Server"

TLS_PATH ?= tls
ROOT_TLS_SUFFIX ?= rootCA

SERVER_KEY_FILE = 41_server.key
SERVER_CERT_FILE = 42_server.pem

ROOT_TLS_OUTPUTS := ${TLS_PATH}/${ROOT_TLS_SUFFIX}.key ${TLS_PATH}/${ROOT_TLS_SUFFIX}.pem
CLIENT_TLS_OUTPUTS := ${TLS_PATH}/client.key ${TLS_PATH}/client.pem
SERVER_TLS_OUTPUTS := ${TLS_PATH}/${SERVER_KEY_FILE} ${TLS_PATH}/${SERVER_CERT_FILE}
X509_CA_FLAGS := -CA "${TLS_PATH}/${ROOT_TLS_SUFFIX}.pem" -CAkey "${TLS_PATH}/${ROOT_TLS_SUFFIX}.key"

# mind the spaces in the args when calling this
# 4 args:= file_suffix, subject, req_section, x509 params
define gen_cert
	openssl genpkey -algorithm ED25519 -out ${TLS_PATH}/$(1).key
	openssl req -new -key ${TLS_PATH}/$(1).key -out ${TLS_PATH}/$(1).csr -section common -subj $(2) -config openssl.cnf
	openssl x509 -req -in ${TLS_PATH}/$(1).csr -out ${TLS_PATH}/$(1).pem -extfile openssl.cnf -extensions $(3) $(4)
	rm -f ${TLS_PATH}/$(1).csr
endef


${ROOT_TLS_OUTPUTS}:
	@$(call gen_cert,${ROOT_TLS_SUFFIX}, ${ROOT_SUBJ}, ca_extensions, -days 30 -key ${TLS_PATH}/${ROOT_TLS_SUFFIX}.key)

${CLIENT_TLS_OUTPUTS}: ${ROOT_TLS_OUTPUTS}
	@$(call gen_cert,client, ${CLIENT_SUBJ}, client_extensions, -days 1 ${X509_CA_FLAGS} )
	cat ${TLS_PATH}/${ROOT_TLS_SUFFIX}.pem >> ${TLS_PATH}/client.pem

${SERVER_TLS_OUTPUTS}: ${ROOT_TLS_OUTPUTS}
	@$(call gen_cert,server, ${SERVER_SUBJ}, server_extensions, -days 1 ${X509_CA_FLAGS} )
	cat ${TLS_PATH}/${ROOT_TLS_SUFFIX}.pem >> ${TLS_PATH}/server.pem
	cd ${TLS_PATH} && \
	mv server.key ${SERVER_KEY_FILE} && \
	mv server.pem ${SERVER_CERT_FILE}

.PHONY: client-tls server-tls root-tls all-tls clean-tls

client-tls: ${CLIENT_TLS_OUTPUTS}

server-tls: ${SERVER_TLS_OUTPUTS}

root-tls: ${ROOT_TLS_OUTPUTS}

all-tls: client-tls server-tls

clean-tls:
	rm -f ${ROOT_TLS_OUTPUTS} ${SERVER_TLS_OUTPUTS} ${CLIENT_TLS_OUTPUTS}


## ssh-key stuff
SSH_KEY_PREFIX ?= ssh_key
SSH_PRIV := vars/${SSH_KEY_PREFIX}.pem
SSH_PUB := vars/${SSH_KEY_PREFIX}.pem.pub
SSH_KEY_OUTPUTS :=  ${SSH_PRIV} ${SSH_PUB}

.PHONY: ssh-key clean-ssh-key

${SSH_PRIV} ${SSH_PUB}:
	ssh-keygen -t ed25519 -f ${SSH_PRIV}

ssh-key: ${SSH_PRIV} ${SSH_PUB}

clean-ssh-key:
	rm -f ${SSH_PRIV} ${SSH_PUB}

.PHONY: all clean

all: ssh-key all-tls

clean: clean-tls clean-ssh-key





