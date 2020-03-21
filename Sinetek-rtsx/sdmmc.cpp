/*	$OpenBSD: sdmmc.c,v 1.45 2017/01/21 05:42:04 guenther Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Host controller independent SD/MMC bus driver based on information
 * from SanDisk SD Card Product Manual Revision 2.2 (SanDisk), SDIO
 * Simple Specification Version 1.0 (SDIO) and the Linux "mmc" driver.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <IOKit/IOTimerEventSource.h>

#include "sdmmcchip.h"
#include "sdmmcreg.h"
#include "sdmmcvar.h"

#include "Sinetek_rtsx.hpp"

void	sdmmc_attach(struct device *, struct device *, void *);
int	sdmmc_detach(struct device *, int);
//int	sdmmc_activate(struct device *, int);

void	sdmmc_create_thread(void *);
void	sdmmc_task_thread(void *);
void	sdmmc_discover_task(void *);
void	sdmmc_card_attach(struct sdmmc_softc *);
void	sdmmc_card_detach(struct sdmmc_softc *, int);
int	sdmmc_enable(struct sdmmc_softc *);
void	sdmmc_disable(struct sdmmc_softc *);
int	sdmmc_scan(struct sdmmc_softc *);
int	sdmmc_init(struct sdmmc_softc *);
#ifdef SDMMC_IOCTL
int	sdmmc_ioctl(struct device *, u_long, caddr_t);
#endif

#ifdef SDMMC_DEBUG
int sdmmcdebug = 0;
extern int sdhcdebug;	/* XXX should have a sdmmc_chip_debug() function */
void sdmmc_dump_command(struct sdmmc_softc *, struct sdmmc_command *);
#define DPRINTF(n,s)	do { printf s; } while (0)
#else
#define DPRINTF(n,s)	do {} while (0)
#endif

extern int splsdmmc();
extern void splx(int);

// must be after the definitions of splsdmmc/splx
#define UTL_THIS_CLASS ""
#include "util.h"

void
sdmmc_attach(struct sdmmc_softc *sc)
{
	int error;
	
	if (ISSET(sc->sc_caps, SMC_CAPS_8BIT_MODE))
		printf(": 8-bit");
	else if (ISSET(sc->sc_caps, SMC_CAPS_4BIT_MODE))
		printf(": 4-bit");
	else
		printf(": 1-bit");
	if (ISSET(sc->sc_caps, SMC_CAPS_SD_HIGHSPEED))
		printf(", sd high-speed");
	if (ISSET(sc->sc_caps, SMC_CAPS_MMC_HIGHSPEED))
		printf(", mmc high-speed");
	if (ISSET(sc->sc_caps, SMC_CAPS_DMA))
		printf(", dma");
	
	if (ISSET(sc->sc_caps, SMC_CAPS_DMA)) {
        printf("%s: sc-scaps if ===>\n", __func__);
		error = ENOTSUP;
		if (error) {
			printf("%s: can't create DMA map\n", DEVNAME(sc));
			return;
		}
        printf("%s: sc-scaps if <===\n", __func__);
	}
	
	STAILQ_INIT(&sc->sf_head);
	TAILQ_INIT(&sc->sc_tskq);
	TAILQ_INIT(&sc->sc_intrq);
	sdmmc_init_task(&sc->sc_discover_task, sdmmc_discover_task, sc);
	
#ifdef SDMMC_IOCTL
	if (bio_register(self, sdmmc_ioctl) != 0)
		printf("%s: unable to register ioctl\n", DEVNAME(sc));
#endif
	
	/*
	 * Create the event thread that will attach and detach cards
	 * and perform other lengthy operations.  Enter config_pending
	 * state until the discovery task has run for the first time.
	 */
    printf("%s: sc->flags ===>\n", __func__);
	SET(sc->sc_flags, SMF_CONFIG_PENDING);
    printf("%s: sc_flags <===\n", __func__);
    printf("sdmmc_attach() <===\n");
}

int
sdmmc_detach(struct device *self, int flags)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
	
	(void)sc;
	
	return 0;
}

//int
//sdmmc_activate(struct device *self, int act)
//{
//	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
//	int rv = 0;
//	
//	switch (act) {
//		case DVACT_SUSPEND:
//			rv = config_activate_children(self, act);
//			/* If card in slot, cause a detach/re-attach */
//			if (ISSET(sc->sc_flags, SMF_CARD_PRESENT))
//				sc->sc_dying = -1;
//			break;
//		case DVACT_RESUME:
//			rv = config_activate_children(self, act);
//			wakeup(&sc->sc_tskq);
//			break;
//		default:
//			rv = config_activate_children(self, act);
//			break;
//	}
//	return (rv);
//}

//void
//sdmmc_create_thread(void *arg)
//{
//	struct sdmmc_softc *sc = arg;
//	
//	if (kthread_create(sdmmc_task_thread, sc, &sc->sc_task_thread,
//			   DEVNAME(sc)) != 0)
//		printf("%s: can't create task thread\n", DEVNAME(sc));
//	
//}

//void
//sdmmc_task_thread(void *arg)
//{
//	struct sdmmc_softc *sc = arg;
//	struct sdmmc_task *task;
//	int s;
//	
//restart:
//	sdmmc_needs_discover(&sc->sc_dev);
//	
//	s = splsdmmc();
//	while (!sc->sc_dying) {
//		for (task = TAILQ_FIRST(&sc->sc_tskq); task != NULL;
//		     task = TAILQ_FIRST(&sc->sc_tskq)) {
//			splx(s);
//			sdmmc_del_task(task);
//			task->func(task->arg);
//			s = splsdmmc();
//		}
//		tsleep(&sc->sc_tskq, PWAIT, "mmctsk", 0);
//	}
//	splx(s);
//	
//	if (ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
//		rw_enter_write(&sc->sc_lock);
//		sdmmc_card_detach(sc, DETACH_FORCE);
//		rw_exit(&sc->sc_lock);
//	}
//	
//	/*
//	 * During a suspend, the card is detached since we do not know
//	 * if it is the same upon wakeup.  Go re-discover the bus.
//	 */
//	if (sc->sc_dying == -1) {
//		CLR(sc->sc_flags, SMF_CARD_PRESENT);
//		sc->sc_dying = 0;
//		goto restart;
//	}
//	sc->sc_task_thread = NULL;
//	wakeup(sc);
//	kthread_exit(0);
//}

// cholonam: This may be called from workloop...
void
sdmmc_add_task(struct sdmmc_softc *sc, struct sdmmc_task *task)
{
	UTL_DEBUG(1, "START");
	int s;
	
	s = splsdmmc();
	// cholonam: a race condition may happen here since an insert may be done by a client request, while a task is
	//           being retrieved by the timer! -> we need to protect sc->sc_tskq
	TAILQ_INSERT_TAIL(&sc->sc_tskq, task, next);
	task->onqueue = 1;
	task->sc = sc;
#if RTSX_USE_IOCOMMANDGATE
    sc->executeOneAsCommand(); // this will run in the task work loop, but we need to exit the workloop_ first!
#elif RTSX_USE_IOLOCK
	
#else
	wakeup(&sc->sc_tskq);
#endif
	sc->task_execute_one_->setTimeoutTicks(100 / 5);
	splx(s);
	UTL_DEBUG(1, "END");
}

void
sdmmc_del_task(struct sdmmc_task *task)
{
    UTL_CHK_PTR(task,);
    UTL_CHK_PTR(task->sc,);
	struct sdmmc_softc *sc = task->sc;
	int s;
	
	if (sc == NULL)
		return;
	
	s = splsdmmc();
	task->sc = NULL;
	task->onqueue = 0;
	TAILQ_REMOVE(&sc->sc_tskq, task, next);
	splx(s);
}

// cholonam: This is called alled from card_insert/card_eject
void
sdmmc_needs_discover(struct device *self)
{
	UTL_DEBUG(0, "START");
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
	
	UTL_DEBUG(0, "Task pending (on queue): %d", sdmmc_task_pending(&sc->sc_discover_task));
	
	if (!sdmmc_task_pending(&sc->sc_discover_task))
		sdmmc_add_task(sc, &sc->sc_discover_task);
	UTL_DEBUG(0, "END");
}

// cholonam: This is called by a card insertion or ejection
void
sdmmc_discover_task(void *arg)
{
	if (!arg) return;
	UTL_DEBUG(1, "START");

	struct sdmmc_softc *sc = static_cast<struct sdmmc_softc *>(arg);
	
	if (rtsx_card_detect(sc)) {
		if (!ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
			SET(sc->sc_flags, SMF_CARD_PRESENT);
			sdmmc_card_attach(sc);
		}
	} else {
		if (ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
			CLR(sc->sc_flags, SMF_CARD_PRESENT);
			sdmmc_card_detach(sc, DETACH_FORCE);
		}
	}
	
	if (ISSET(sc->sc_flags, SMF_CONFIG_PENDING)) {
		UTL_DEBUG(1, "3...");
		CLR(sc->sc_flags, SMF_CONFIG_PENDING);
//		config_pending_decr();
		printf("sdmmc: config_pending_decr() XXX\n");
	}
	UTL_DEBUG(1, "END");
}

/*
 * Called from process context when a card is present.
 */
void
sdmmc_card_attach(struct sdmmc_softc *sc)
{
	DPRINTF(1,("%s: attach card\n", DEVNAME(sc)));
	
	CLR(sc->sc_flags, SMF_CARD_ATTACHED);
	
	/*
	 * Power up the card (or card stack).
	 */
	if (sdmmc_enable(sc) != 0) {
		printf("%s: can't enable card\n", DEVNAME(sc));
		goto err;
	}
	
	/*
	 * Scan for I/O functions and memory cards on the bus,
	 * allocating a sdmmc_function structure for each.
	 */
	if (sdmmc_scan(sc) != 0) {
		printf("%s: no functions\n", DEVNAME(sc));
		goto err;
	}
	
	/*
	 * Initialize the I/O functions and memory cards.
	 */
	if (sdmmc_init(sc) != 0) {
		printf("%s: init failed\n", DEVNAME(sc));
		goto err;
	}
	
	/* Attach SCSI emulation for memory cards. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sc->blk_attach();
	
	/* Attach I/O function drivers. */
	if (ISSET(sc->sc_flags, SMF_IO_MODE))
		sdmmc_io_attach(sc);
	
	SET(sc->sc_flags, SMF_CARD_ATTACHED);
	return;
err:
	sdmmc_card_detach(sc, DETACH_FORCE);
}

/*
 * Called from process context with DETACH_* flags from <sys/device.h>
 * when cards are gone.
 */
void
sdmmc_card_detach(struct sdmmc_softc *sc, int flags)
{
	struct sdmmc_function *sf, *sfnext;
	
	DPRINTF(1,("%s: detach card\n", DEVNAME(sc)));
	
	if (ISSET(sc->sc_flags, SMF_CARD_ATTACHED)) {
		/* Detach I/O function drivers. */
		if (ISSET(sc->sc_flags, SMF_IO_MODE))
			sdmmc_io_detach(sc);
		
		/* Detach the SCSI emulation for memory cards. */
		if (ISSET(sc->sc_flags, SMF_MEM_MODE))
			sc->blk_detach();
		
		CLR(sc->sc_flags, SMF_CARD_ATTACHED);
	}
	
	/* Power down. */
	sdmmc_disable(sc);
	
	/* Free all sdmmc_function structures. */
	for (sf = STAILQ_FIRST(&sc->sf_head); sf != NULL; sf = sfnext) {
		sfnext = STAILQ_NEXT(sf, sf_list);
		sdmmc_function_free(sf);
	}
	STAILQ_INIT(&sc->sf_head);
	sc->sc_function_count = 0;
	sc->sc_fn0 = NULL;
}

int
sdmmc_enable(struct sdmmc_softc *sc)
{
	u_int32_t host_ocr;
	int error;
	
	/*
	 * Calculate the equivalent of the card OCR from the host
	 * capabilities and select the maximum supported bus voltage.
	 */
	host_ocr = rtsx_host_ocr(sc);
	error = rtsx_bus_power(sc, host_ocr);
	if (error != 0) {
		printf("%s: can't supply bus power\n", DEVNAME(sc));
		goto err;
	}
	
	/*
	 * Select the minimum clock frequency.
	 */
	error = rtsx_bus_clock(sc,
				     SDMMC_SDCLK_400KHZ, SDMMC_TIMING_LEGACY);
	if (error != 0) {
		printf("%s: can't supply clock\n", DEVNAME(sc));
		goto err;
	}
	
	/* XXX wait for card to power up */
	sdmmc_delay(250000);
	
	/* Initialize SD I/O card function(s). */
	if ((error = sdmmc_io_enable(sc)) != 0)
		goto err;
	
	/* Initialize SD/MMC memory card(s). */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
	    (error = sdmmc_mem_enable(sc)) != 0)
		goto err;
	
err:
	if (error != 0)
		sdmmc_disable(sc);
	
	return error;
}

void
sdmmc_disable(struct sdmmc_softc *sc)
{
	/* XXX complete commands if card is still present. */
	
	/* Make sure no card is still selected. */
	(void)sdmmc_select_card(sc, NULL);
	
	/* Turn off bus power and clock. */
	(void)rtsx_bus_clock(sc,
				   SDMMC_SDCLK_OFF, SDMMC_TIMING_LEGACY);
	(void)rtsx_bus_power(sc, 0);
}

/*
 * Set the lowest bus voltage supported by the card and the host.
 */
int
sdmmc_set_bus_power(struct sdmmc_softc *sc, u_int32_t host_ocr,
		    u_int32_t card_ocr)
{
	u_int32_t bit;
	
	/* Mask off unsupported voltage levels and select the lowest. */
	DPRINTF(1,("%s: host_ocr=%x ", DEVNAME(sc), host_ocr));
	host_ocr &= card_ocr;
	for (bit = 4; bit < 23; bit++) {
		if (ISSET(host_ocr, 1<<bit)) {
			host_ocr &= 3<<bit;
			break;
		}
	}
	DPRINTF(1,("card_ocr=%x new_ocr=%x\n", card_ocr, host_ocr));
	
	if (host_ocr == 0 ||
	    rtsx_bus_power(sc, host_ocr) != 0)
		return 1;
	return 0;
}

struct sdmmc_function *
sdmmc_function_alloc(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;
	
//	sf = (struct sdmmc_function *)malloc(sizeof *sf, M_DEVBUF,
//					     M_WAITOK | M_ZERO);
	sf= (struct sdmmc_function *) new uint8_t[sizeof *sf] ();
	sf->sc = sc;
	sf->number = -1;
	sf->cis.manufacturer = SDMMC_VENDOR_INVALID;
	sf->cis.product = SDMMC_PRODUCT_INVALID;
	sf->cis.function = SDMMC_FUNCTION_INVALID;
	return sf;
}

void
sdmmc_function_free(struct sdmmc_function *sf)
{
//	free(sf, M_DEVBUF, 0);
	delete [] (uint8_t *) sf;
}

/*
 * Scan for I/O functions and memory cards on the bus, allocating a
 * sdmmc_function structure for each.
 */
int
sdmmc_scan(struct sdmmc_softc *sc)
{
	
	/* Scan for I/O functions. */
	if (ISSET(sc->sc_flags, SMF_IO_MODE))
		sdmmc_io_scan(sc);
	
	/* Scan for memory cards on the bus. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_mem_scan(sc);
	
	/* There should be at least one function now. */
	if (STAILQ_EMPTY(&sc->sf_head)) {
		printf("%s: can't identify card\n", DEVNAME(sc));
		return 1;
	}
	return 0;
}

/*
 * Initialize all the distinguished functions of the card, be it I/O
 * or memory functions.
 */
int
sdmmc_init(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;
	
	/* Initialize all identified card functions. */
	STAILQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (ISSET(sc->sc_flags, SMF_IO_MODE) &&
		    sdmmc_io_init(sc, sf) != 0)
			printf("%s: i/o init failed\n", DEVNAME(sc));
		
		if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
		    sdmmc_mem_init(sc, sf) != 0)
			printf("%s: mem init failed\n", DEVNAME(sc));
	}
	
	/* Any good functions left after initialization? */
	STAILQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (!ISSET(sf->flags, SFF_ERROR))
			return 0;
	}
	/* No, we should probably power down the card. */
	return 1;
}

void
sdmmc_delay(u_int usecs)
{
	IODelay(usecs);
}

int
sdmmc_app_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	struct sdmmc_command acmd;
	int error;
	
	bzero(&acmd, sizeof acmd);
	acmd.c_opcode = MMC_APP_CMD;
	acmd.c_arg = 0;
	if (sc->sc_card != NULL) {
		acmd.c_arg = sc->sc_card->rca << 16;
	}
	acmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	
	error = sdmmc_mmc_command(sc, &acmd);
	if (error != 0) {
		return error;
	}
	
	if (!ISSET(MMC_R1(acmd.c_resp), MMC_R1_APP_CMD)) {
		/* Card does not support application commands. */
		return ENODEV;
	}
	
	error = sdmmc_mmc_command(sc, cmd);
	return error;
}

/*
 * Execute MMC command and data transfers.  All interactions with the
 * host controller to complete the command happen in the context of
 * the current process.
 */
int
sdmmc_mmc_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	int error;
	
	rtsx_exec_command(sc, cmd);
	
#ifdef SDMMC_DEBUG
	sdmmc_dump_command(sc, cmd);
#endif
	
	error = cmd->c_error;
	wakeup(cmd);
	
	return error;
}

/*
 * Send the "GO IDLE STATE" command.
 */
void
sdmmc_go_idle_state(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;
	
	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_GO_IDLE_STATE;
	cmd.c_flags = SCF_CMD_BC | SCF_RSP_R0;
	
	(void)sdmmc_mmc_command(sc, &cmd);
}

/*
 * Send the "SEND_IF_COND" command, to check operating condition
 */
int
sdmmc_send_if_cond(struct sdmmc_softc *sc, uint32_t card_ocr)
{
	struct sdmmc_command cmd;
	uint8_t pat = 0x23;	/* any pattern will do here */
	uint8_t res;
	
	bzero(&cmd, sizeof cmd);
	
	cmd.c_opcode = SD_SEND_IF_COND;
	cmd.c_arg = ((card_ocr & SD_OCR_VOL_MASK) != 0) << 8 | pat;
	cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R7;
	
	if (sdmmc_mmc_command(sc, &cmd) != 0)
		return 1;
	
	res = cmd.c_resp[0];
	if (res != pat)
		return 1;
	else
		return 0;
}

/*
 * Retrieve (SD) or set (MMC) the relative card address (RCA).
 */
int
sdmmc_set_relative_addr(struct sdmmc_softc *sc,
			struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	
	bzero(&cmd, sizeof cmd);
	
	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R6;
	} else {
		cmd.c_opcode = MMC_SET_RELATIVE_ADDR;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	}
	
	if (sdmmc_mmc_command(sc, &cmd) != 0)
		return 1;
	
	if (ISSET(sc->sc_flags, SMF_SD_MODE))
		sf->rca = SD_R6_RCA(cmd.c_resp);
	return 0;
}

int
sdmmc_select_card(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	int error;
	
	if (sc->sc_card == sf || (sf && sc->sc_card &&
				  sc->sc_card->rca == sf->rca)) {
		sc->sc_card = sf;
		return 0;
	}
	
	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_SELECT_CARD;
	cmd.c_arg = sf == NULL ? 0 : MMC_ARG_RCA(sf->rca);
	cmd.c_flags = SCF_CMD_AC | (sf == NULL ? SCF_RSP_R0 : SCF_RSP_R1);
	error = sdmmc_mmc_command(sc, &cmd);
	if (error == 0 || sf == NULL)
		sc->sc_card = sf;
	return error;
}

#ifdef SDMMC_IOCTL
int
sdmmc_ioctl(struct device *self, u_long request, caddr_t addr)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
	struct sdmmc_command *ucmd;
	struct sdmmc_command cmd;
	void *data;
	int error = 0;
	
	switch (request) {
#ifdef SDMMC_DEBUG
		case SDIOCSETDEBUG:
			sdmmcdebug = (((struct bio_sdmmc_debug *)addr)->debug) & 0xff;
			sdhcdebug = (((struct bio_sdmmc_debug *)addr)->debug >> 8) & 0xff;
			break;
#endif
			
		case SDIOCEXECMMC:
		case SDIOCEXECAPP:
			ucmd = &((struct bio_sdmmc_command *)addr)->cmd;
			
			/* Refuse to transfer more than 512K per command. */
			if (ucmd->c_datalen > 524288)
				return ENOMEM;
			
			/* Verify that the data buffer is safe to copy. */
			if ((ucmd->c_datalen > 0 && ucmd->c_data == NULL) ||
			    (ucmd->c_datalen < 1 && ucmd->c_data != NULL) ||
			    ucmd->c_datalen < 0)
				return EINVAL;
			
			bzero(&cmd, sizeof cmd);
			cmd.c_opcode = ucmd->c_opcode;
			cmd.c_arg = ucmd->c_arg;
			cmd.c_flags = ucmd->c_flags;
			cmd.c_blklen = ucmd->c_blklen;
			
			if (ucmd->c_data) {
				data = malloc(ucmd->c_datalen, M_TEMP,
					      M_WAITOK | M_CANFAIL);
				if (data == NULL)
					return ENOMEM;
				error = copyin(ucmd->c_data, data, ucmd->c_datalen);
				if (error != 0)
					goto exec_done;
				
				cmd.c_data = data;
				cmd.c_datalen = ucmd->c_datalen;
			}
			
			rw_enter_write(&sc->sc_lock);
			if (request == SDIOCEXECMMC)
				error = sdmmc_mmc_command(sc, &cmd);
			else
				error = sdmmc_app_command(sc, &cmd);
			rw_exit(&sc->sc_lock);
			if (error && !cmd.c_error)
				cmd.c_error = error;
			
			bcopy(&cmd.c_resp, ucmd->c_resp, sizeof cmd.c_resp);
			ucmd->c_flags = cmd.c_flags;
			ucmd->c_error = cmd.c_error;
			
			if (ucmd->c_data)
				error = copyout(data, ucmd->c_data, ucmd->c_datalen);
			else
				error = 0;
			
		exec_done:
			if (ucmd->c_data)
				free(data, M_TEMP, 0);
			break;
			
		default:
			return ENOTTY;
	}
	return error;
}
#endif

#ifdef SDMMC_DEBUG
void
sdmmc_dump_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	int i;
	
	DPRINTF(1,("%s: cmd %u arg=%#x data=%p dlen=%d flags=%#x "
	    "proc=\"%s\" (error %d)\n", DEVNAME(sc), cmd->c_opcode,
	    cmd->c_arg, cmd->c_data, cmd->c_datalen, cmd->c_flags,
	    "", cmd->c_error));
    if (cmd->c_error) {
        UTL_ERR("%s: cmd %u (%s) arg=%#x data=%p dlen=%d flags=%#x proc=\"%s\" "
                "(error %d)\n", DEVNAME(sc), cmd->c_opcode, mmcCmd2str(cmd->c_opcode),
                cmd->c_arg, cmd->c_data, cmd->c_datalen, cmd->c_flags, "", cmd->c_error);
    } else {
        UTL_DEBUG(2, "%s: cmd %u (%s) arg=%#x data=%p dlen=%d flags=%#x proc=\"%s\" "
                  "(error %d)\n", DEVNAME(sc), cmd->c_opcode, mmcCmd2str(cmd->c_opcode),
                  cmd->c_arg, cmd->c_data, cmd->c_datalen, cmd->c_flags, "", cmd->c_error);
    }
	
	if (cmd->c_error || sdmmcdebug < 1)
		return;
	
	printf("%s: resp=", DEVNAME(sc));
	if (ISSET(cmd->c_flags, SCF_RSP_136))
		for (i = 0; i < sizeof cmd->c_resp; i++)
			printf("%02x ", ((u_char *)cmd->c_resp)[i]);
	else if (ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		for (i = 0; i < 4; i++)
			printf("%02x ", ((u_char *)cmd->c_resp)[i]);
	printf("\n");
}
#endif
