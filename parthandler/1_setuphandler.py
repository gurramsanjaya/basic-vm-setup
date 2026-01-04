
## using https://github.com/canonical/cloud-init/
from cloudinit import handlers, subp, templater, util
import re
import os
import ipaddress
import logging

LOG = logging.getLogger(__name__)

# version 3 calls render_part with frequency
handler_version = 2

# config vars determined at runtime for templates, add other vars as needed
render_vars = {}
DEF_INTERFACE = "default_interface"
EXTERNAL_IP = "external_ip"

# default ip to check default network device
DEF_IP_TO_CHECK = "9.9.9.9"

# mime types
NFT_TYPE = "text/x-custom-nft"
WGE_TYPE = "text/x-custom-wge"
WG_TYPE = "text/x-custom-wg"

# param
RENDER_PARAM = "render"

# paths
CONTENT_TYPE_PATH_MAP = {
    NFT_TYPE: "/etc/",
    WGE_TYPE: "/etc/wge/",
    WG_TYPE: "/etc/wireguard/"
}

NFTABLE = "nftables.conf"
nftable_count = 0

DEF_MODE = 0o640

def list_types():
    return list(CONTENT_TYPE_PATH_MAP.keys())

# copied this from templater.py
def isExplicitTemplate(text: str):
    if text.find("\n") != -1:
        ident, _ = text.split("\n", 1)  # remove the first line
    else:
        ident = text
    type_match = re.match(r"##\s*template:\s*?(jinja|basic)\s*", ident, re.I)
    if (not type_match) or (not type_match.group(1)):
        return False
    return True

# copied this from boot_hook.py
def write_part(dirpath:str, filename: str, payload: str, mode: int) -> str:
    filename = util.clean_filename(filename)
    dir_path = os.path.realpath(dirpath)
    filepath = os.path.join(dir_path, filename)
    util.write_file(os.path.join(dir_path, filename), payload, mode)
    return filepath


# all part handlers run after network config
def handle_part(data: any, ctype: str, filename: str, payload: str, frequency: any):
    global render_vars

    if ctype == handlers.CONTENT_START:
        LOG.info("initiating...")

        out, _ = subp.subp(["ip", "route", "get", DEF_IP_TO_CHECK], rcs=[0])
        assert isinstance(out, str)
        m = re.search(r"dev\s(\w*)", out.rstrip(), re.RegexFlag.M)
        render_vars[DEF_INTERFACE] = m.group(1)
        if not render_vars[DEF_INTERFACE]:
            raise RuntimeError("invalid device match")

        out, _ = subp.subp(["curl", "-4", "--interface", render_vars[DEF_INTERFACE], "https://icanhazip.com"], rcs=[0])
        assert isinstance(out, str)
        ip = ipaddress.ip_address(out.strip())
        render_vars[EXTERNAL_IP] = ip.compressed

        LOG.info(f"intiation complete with vars: {render_vars}")
        return

    if ctype == handlers.CONTENT_END:
        return

    if isExplicitTemplate(payload):
        LOG.info(f"template detected in filename: {filename}, content_type: {ctype}")
        # follows cloud init jinja/basic rendering logic
        payload = templater.render_string(payload, render_vars)

    if ctype in CONTENT_TYPE_PATH_MAP:
        LOG.info(f"for filename: {filename}, content_type: {ctype} detected, handling...")
        if ctype == NFT_TYPE:
            global nftable_count
            if filename == NFTABLE and nftable_count == 0:
                nftable_count += 1
                nft_conf = write_part(CONTENT_TYPE_PATH_MAP[ctype], NFTABLE, payload, DEF_MODE)
                subp.subp(["nft", "-f", nft_conf], rcs=[0])
            else:
                raise ValueError(f"failure to set nftables.conf, filename: {filename}, count: {nftable_count}")
        else:
            write_part(CONTENT_TYPE_PATH_MAP[ctype], filename, payload, DEF_MODE)
    return