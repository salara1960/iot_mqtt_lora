#
# Main Makefile. This is basically the same as a component makefile.
#

#COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS :=  .

CRTDIR := certs
#TLSDIR := new

ifdef CONFIG_EXAMPLE_EMBEDDED_CERTS
# Certificate files. certificate.pem.crt & private.pem.key must be downloaded
# from AWS, see README for details.
COMPONENT_EMBED_TXTFILES := $(CRTDIR)/ca.crt $(CRTDIR)/cacert.pem $(CRTDIR)/cakey.pem
#$(TLSDIR)/tlscert.pem $(TLSDIR)/tlskey.pem


endif
