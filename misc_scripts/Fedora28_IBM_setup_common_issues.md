##### WST Compliance Account Configuration Problems
Use `chage --maxdays 90 <username>` to set password change policy every 90 days.

-- For error `import gtk,gtk.glade module not found`: dnf install pygtk2-libglade
-- [New Joinee to LTC setup guidelines](https://ltc3.linux.ibm.com/wiki/IndiaTeam/Newjoinees#head-c41868fe74b033cc5bbd13336332694c387cb4d6)

#### SAS VPN Process:
###### _How to configure SAS VPN on Fedora 28:_
1. Install `ibm-config-NetworkManager-openconnect`
2. Generate your PKCS certificate (`.p12`) using the steps from https://w3.ibm.com/help/#/article/linux_sas_vpn
3. Download the certificate, e.g. `ibm-vpn-linux.p12`, and run `openssl pkcs12 -in ibm-vpn-linux.p12 -out ibm-vpn-linux.pem -nodes` to convert it to PEM format as GNOME Network Manager cannot understand PKCS
4. Go to `Settings->Network` and add a new VPN connection
5. For type, select `Cisco AnyConnect Compatible VPN (openconnect)`
6. Set `VPN Protocol` to `Cisco AnyConnect`
7. Set `Gateway` to `sasvpn.in.ibm.com`
8. Set `CA Certificate` to the PEM certificate generated in step 3, e.g. `ibm-vpn-linux.pem`
9. Check `Allow Cisco Secure Desktop trojan`
10. Set `CSD Wrapper Script` to `/usr/share/ibm-config-NetworkManager-openconnect/ohsd.py`
11. Set `User Certificate` to the same PEM certificate
12. Set `Private Key` also to the same PEM certificate
13. Keep the defaults for everything else

