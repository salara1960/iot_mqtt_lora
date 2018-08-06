#!/bin/sh

#openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt

openssl req -x509 -newkey rsa:4096 -keyout cakey.pem -out cacert.pem -days 365
