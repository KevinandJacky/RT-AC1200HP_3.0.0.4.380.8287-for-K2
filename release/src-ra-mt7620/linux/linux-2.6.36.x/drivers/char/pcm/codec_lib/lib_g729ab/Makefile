rm=/bin/rm -f
cp=/bin/cp -f
mkdir=/bin/mkdir -p 

CPU_NAME= mips
CROSS_DEV_DIR = /opt/sde6
CROSS_TOOLS = sde
TOOLDIR = $(CROSS_DEV_DIR)/bin

CC	= $(TOOLDIR)/$(CROSS_TOOLS)-gcc
AR 	= $(TOOLDIR)/$(CROSS_TOOLS)-ar cq

G729_DIR = .

g729ab_obj = $(G729_DIR)/g729abcoder_api.o $(G729_DIR)/g729abdecoder_api.o $(G729_DIR)/cod_struct.o $(G729_DIR)/dec_struct.o\
 $(G729_DIR)/acelp_ca.o\
 $(G729_DIR)/de_acelp.o $(G729_DIR)/basic_op.o \
 $(G729_DIR)/bits.o $(G729_DIR)/cod_ld8a.o $(G729_DIR)/dec_ld8a.o \
 $(G729_DIR)/dspfunc.o $(G729_DIR)/filter.o $(G729_DIR)/gainpred.o $(G729_DIR)/lpc.o \
 $(G729_DIR)/lpcfunc.o $(G729_DIR)/dec_gain.o\
 $(G729_DIR)/lspdec.o $(G729_DIR)/lspgetq.o $(G729_DIR)/oper_32b.o \
 $(G729_DIR)/p_parity.o $(G729_DIR)/pitch_a.o $(G729_DIR)/pre_proc.o $(G729_DIR)/post_pro.o\
 $(G729_DIR)/dec_lag3.o $(G729_DIR)/pred_lt3.o $(G729_DIR)/qua_gain.o $(G729_DIR)/qua_lsp.o \
 $(G729_DIR)/tab_ld8a.o $(G729_DIR)/util.o\
 $(G729_DIR)/taming.o $(G729_DIR)/cor_func.o $(G729_DIR)/vad.o $(G729_DIR)/tab_dtx.o $(G729_DIR)/dtx.o\
 $(G729_DIR)/qsidgain.o $(G729_DIR)/qsidlsf.o $(G729_DIR)/calcexc.o $(G729_DIR)/dec_sid.o $(G729_DIR)/postfilt.o	\
	$(G729_DIR)/g729_var.o
	
ase_obj = \
			basic_ops.o	\
			oper_32bs.o	\
			acelp_cas.o \
			filters.o
			
OBJS		= $(g729ab_obj) $(ase_obj) 
VPATH +=$(G729_DIR) $(G729_DIR)/ase
CFLAGS =-c -O1 -G0 -EL -Wall -Wstrict-prototypes -Wno-trigraphs -mabi=32 -fomit-frame-pointer

CFLAGS +=-ffunction-sections -fdata-sections
CFLAGS +=-fmerge-all-constants -mno-embedded-data -mno-data-in-code
CFLAGS +=-mno-abicalls -fno-builtin -fno-pic -pipe -mips32r2 -mtune=24kec
CFLAGS +=-D__unix__  -fno-pic #-DRALINK_G729AB_VERIFY #-DRALINK_VOIP_CVERSION

%.o : %.S
	$(CC) -c $(CFLAGS) $<
	
%.o : %.c
	$(CC) -c $(CFLAGS) $<
		
all: lib_g729ab.a
lib_g729ab.a: $(OBJS)
	$(AR) $@ $(OBJS)
	#$(RANLIB) $@		

clean:
	$(rm) $(OBJS) lib_g729ab.a core *~	
