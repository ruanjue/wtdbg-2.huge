/*
 * 
 * Copyright (c) 2018, Jue Ruan <ruanjue@gmail.com>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRI_PO_MSA_CNS_RJ_H
#define TRI_PO_MSA_CNS_RJ_H

#include "poacns.h"
#include "ksw.h"

typedef struct {
	SeqBank *seqs;
	int8_t matrix[16];
	int fail_skip;
	u1i ksize; // 11
	float kdup; // 0.1
	float keqs; // 0.2
	uuhash *khash;
	u1v *qry, *ref;
	u2i winlen, winmin;
	u2v *regs[2];
	POG *pogs[3];
	BaseBank *cns;
	int is_tripog;
} TriPOG;

static inline TriPOG* init_tripog(int winlen, int winmin, int fail_skip, int M, int X, int I, int D, int W, int use_sse, int rW, u4i min_cnt, float min_freq){
	TriPOG *tp;
	u4i i;
	tp = malloc(sizeof(TriPOG));
	tp->seqs = init_seqbank();
	tp->ksize = 11;
	tp->kdup = 0.1;
	tp->keqs = 0.2;
	tp->khash = init_uuhash(13);
	tp->ref  = init_u1v(winlen);
	tp->qry  = init_u1v(1024);
	tp->regs[0] = init_u2v(32);
	tp->regs[1] = init_u2v(32);
	tp->fail_skip = fail_skip;
	if(winlen > 0){
		tp->winlen = winlen;
		tp->winmin = winmin;
		tp->pogs[0] = init_pog(M, X, I, D, 0, 0, use_sse, 0, min_cnt, min_freq);
	} else {
		tp->winlen = 0;
		tp->winmin = 0;
		tp->pogs[0] = init_pog(M, X, I, D, W, - winlen, use_sse, rW, min_cnt, min_freq);
	}
	tp->pogs[1] = init_pog(M, X, I, D, W, 0, use_sse, rW, min_cnt, min_freq);
	tp->pogs[2] = init_pog(M, X, I, D, W, 0, use_sse, rW, min_cnt, min_freq);
	//tp->pogs[1]->near_dialog = 1;
	//tp->pogs[2]->near_dialog = 1;
	tp->cns = init_basebank();
	for(i=0;i<16;i++){
		tp->matrix[i] = ((i / 4) == (i % 4))? M : X;
	}
	tp->is_tripog = 0;
	return tp;
}

static inline void free_tripog(TriPOG *tp){
	free_seqbank(tp->seqs);
	free_uuhash(tp->khash);
	free_u1v(tp->qry);
	free_u1v(tp->ref);
	free_u2v(tp->regs[0]);
	free_u2v(tp->regs[1]);
	free_pog(tp->pogs[0]);
	free_pog(tp->pogs[1]);
	free_pog(tp->pogs[2]);
	free_basebank(tp->cns);
	free(tp);
}

static inline void beg_tripog(TriPOG *tp){
	clear_seqbank(tp->seqs);
	if(tp->winlen == 0){
		beg_pog(tp->pogs[0]);
	}
	tp->is_tripog = 0;
}

static inline void push_tripog(TriPOG *tp, char *seq, u2i len){
	if(tp->winlen){
		push_seqbank(tp->seqs, NULL, 0, seq, len);
	} else {
		push_seqbank(tp->seqs, NULL, 0, "", 0);
		push_pog(tp->pogs[0], seq, len);
	}
}

static inline void direct_run_tripog(TriPOG *tp){
	POG *g;
	u4i ridx;
	clear_basebank(tp->cns);
	tp->pogs[0]->W = tp->pogs[1]->W;
	tp->pogs[0]->rW = tp->pogs[1]->rW;
	tp->pogs[0]->W_score = tp->pogs[1]->W * tp->pogs[1]->M * 8;
	g = tp->pogs[0];
	beg_pog(g);
	for(ridx=0;ridx<tp->seqs->nseq;ridx++){
		fwdbitpush_pog(g, tp->seqs->rdseqs->bits, tp->seqs->rdoffs->buffer[ridx], tp->seqs->rdlens->buffer[ridx]);
	}
	end_pog(g);
	fast_fwdbits2basebank(tp->cns, g->cns->bits, 0, g->cns->size);
	tp->pogs[0]->W = 0;
	tp->pogs[0]->rW = 0;
	tp->pogs[0]->W_score = 0;
}

static inline void shuffle_reads_tripog(SeqBank *sb, uuhash *khash, u1i ksize){
	f4v *kords;
	u4v *kidxs;
	uuhash_t *u;
	u8i roff;
	u4i ridx, i, kmer, kmask, rlen, khit;
	int exists;
	clear_uuhash(khash);
	kmask = MAX_U4 >> ((16 - ksize) << 1);
	for(ridx=0;ridx<sb->nseq;ridx++){
		rlen = sb->rdlens->buffer[ridx];
		kmer = 0;
		roff = sb->rdoffs->buffer[ridx];
		for(i=0;i<rlen;i++){
			kmer = ((kmer << 2) | get_basebank(sb->rdseqs, roff + i)) & kmask;
			if(i + 1 < ksize) continue;
			u = prepare_uuhash(khash, kmer, &exists);
			if(exists){
				u->val ++;
			} else {
				u->key = kmer;
				u->val = 1;
			}
		}
	}
	kords = init_f4v(sb->nseq);
	kidxs = init_u4v(sb->nseq);
	for(ridx=0;ridx<sb->nseq;ridx++){
		rlen = sb->rdlens->buffer[ridx];
		kmer = 0;
		roff = sb->rdoffs->buffer[ridx];
		khit = 0;
		for(i=0;i<rlen;i++){
			kmer = ((kmer << 2) | get_basebank(sb->rdseqs, roff + i)) & kmask;
			if(i + 1 < ksize) continue;
			u = get_uuhash(khash, kmer);
			if(u->val > 1){
				khit ++;
			}
		}
		push_f4v(kords, ((double)khit) / rlen);
		push_u4v(kidxs, ridx);
	}
	sort_array(kidxs->buffer, kidxs->size, u4i, num_cmpgt(kords->buffer[b], kords->buffer[a]));
	if(cns_debug > 1){
		for(i=0;i<kidxs->size;i++){
			fprintf(stderr, "SHUFFLE[%u] %u\t%0.4f\n", i, kidxs->buffer[i], kords->buffer[kidxs->buffer[i]]);
		}
	}
	free_f4v(kords);
	for(i=0;i<kidxs->size;i++){
		push_u8v(sb->rdoffs, sb->rdoffs->buffer[kidxs->buffer[i]]);
		push_u4v(sb->rdlens, sb->rdlens->buffer[kidxs->buffer[i]]);
	}
	remove_array_u8v(sb->rdoffs, 0, kidxs->size);
	remove_array_u4v(sb->rdlens, 0, kidxs->size);
	free_u4v(kidxs);
}

static inline void end_tripog(TriPOG *tp){
	POG *g;
	kswr_t R;
	u4i ridx, rlen, b, e, failed;
	if(tp->winlen == 0){
		shuffle_reads_tripog(tp->pogs[0]->seqs, tp->khash, tp->ksize);
		end_pog(tp->pogs[0]);
		clear_basebank(tp->cns);
		fast_fwdbits2basebank(tp->cns, tp->pogs[0]->cns->bits, 0, tp->pogs[0]->cns->size);
		return;
	}
	shuffle_reads_tripog(tp->seqs, tp->khash, tp->ksize);
	if(tp->seqs->nseq < 2){
		return direct_run_tripog(tp);
	}
	rlen = tp->seqs->rdlens->buffer[0];
	if(rlen < 2 * tp->winlen){
		return direct_run_tripog(tp);
	}
	clear_u2v(tp->regs[0]);
	clear_u2v(tp->regs[1]);
	// selecting unique window
	{
		uuhash_t *u;
		u4i i, j, hit, nb, kmer, kcnt, kdup, keqs, ktot, kmask, roff;
		kmask = MAX_U4 >> ((16 - tp->ksize) << 1);
		for(ridx=0;ridx<1;ridx++){
			rlen = tp->seqs->rdlens->buffer[ridx];
			kmer = 0;
			roff = tp->seqs->rdoffs->buffer[ridx];
			for(i=0;i<rlen;i++){
				kmer = ((kmer << 2) | get_basebank(tp->seqs->rdseqs, roff + i)) & kmask;
				if(i + 1 < tp->ksize) continue;
				u = get_uuhash(tp->khash, kmer);
				u->val += 1U << 16;
			}
		}
		nb = ((rlen - tp->winlen) * 2 / 3 / tp->winlen / 2) * 2;
		b = (rlen - tp->winlen) / 2;
		hit = 0;
		roff = tp->seqs->rdoffs->buffer[0];
		for(j=0;j<nb;){
			e = b + tp->winlen;
			kmer = 0;
			kdup = 0;
			keqs = 0;
			ktot = e - b + 1 - tp->ksize;
			for(i=b;i<e;i++){
				kmer = ((kmer << 2) | get_basebank(tp->seqs->rdseqs, roff + i)) & kmask;
				if(i + 1 - b < tp->ksize) continue;
				kcnt = getval_uuhash(tp->khash, kmer);
				if((kcnt >> 16) > 1){
					kdup ++;
				} else if((kcnt & 0xFFFFU) > 1){
					keqs ++;
				}
			}
			if(cns_debug > 1){
				fprintf(stderr, "Selecting anchor[%4d,%4d]: ktot=%d keqs=%d kdup=%d\n", b, e, ktot, keqs, kdup);
			}
			if(kdup < UInt(tp->kdup * ktot) && keqs >= UInt(tp->keqs * ktot)){
				hit = 1;
				break;
			}
			j ++;
			if(j & 0x01){
				b = (rlen - tp->winlen) / 2 + tp->winlen * ((j + 1) >> 1);
			} else {
				b = (rlen - tp->winlen) / 2 - tp->winlen * ((j + 1) >> 1);
			}
		}
		if(hit == 0){
			return direct_run_tripog(tp);
		}
	}
	push_u2v(tp->regs[0], b);
	push_u2v(tp->regs[1], e);
	failed = 0;
	for(ridx=1;ridx<tp->seqs->nseq;ridx++){
		clear_and_encap_u1v(tp->qry, tp->winlen);
		bitseq_basebank(tp->seqs->rdseqs, tp->seqs->rdoffs->buffer[0] + b, tp->winlen, tp->qry->buffer);
		rlen = tp->seqs->rdlens->buffer[ridx];
		clear_and_encap_u1v(tp->ref, rlen);
		bitseq_basebank(tp->seqs->rdseqs, tp->seqs->rdoffs->buffer[ridx], rlen, tp->ref->buffer);
		R = ksw_align(tp->winlen, tp->qry->buffer, rlen, tp->ref->buffer, 4, tp->matrix, - (tp->pogs[0]->I + tp->pogs[0]->D)/2, 1, KSW_XSTART, NULL);
		if(R.qb <= -1 || R.tb <= -1 || R.qe <= -1 || R.te <= -1){
			if(cns_debug > 1){
				fprintf(stderr, "FAILED_ALIGN: READ%u [%d,%d=%d][%d,%d=%d]\n", ridx, R.qb, R.qe, R.qe - R.qb, R.tb, R.te, R.te - R.tb);
			}
			if(tp->fail_skip){
				push_u2v(tp->regs[0], MAX_U2);
				push_u2v(tp->regs[1], MAX_U2);
				failed ++;
				continue;
			} else {
				return direct_run_tripog(tp);
			}
		}
		if(R.qe + 1 - R.qb < tp->winmin || R.te + 1 - R.tb < tp->winmin){
			if(cns_debug > 1){
				fprintf(stderr, "FAILED_ALIGN: READ%u [%d,%d=%d][%d,%d=%d]\n", ridx, R.qb, R.qe, R.qe - R.qb, R.tb, R.te, R.te - R.tb);
			}
			if(tp->fail_skip){
				push_u2v(tp->regs[0], MAX_U2);
				push_u2v(tp->regs[1], MAX_U2);
				failed ++;
				continue;
			} else {
				return direct_run_tripog(tp);
			}
		}
		push_u2v(tp->regs[0], R.tb);
		push_u2v(tp->regs[1], R.te + 1);
	}
	if(failed * 2 >= tp->seqs->nseq){
		return direct_run_tripog(tp);
	}
	// building cns for fast aligned regions
	g = tp->pogs[0];
	g->aln_mode = POG_ALNMODE_GLOBAL;
	beg_pog(g);
	for(ridx=0;ridx<tp->seqs->nseq;ridx++){
		if(tp->regs[0]->buffer[ridx] == MAX_U2) continue;
		fwdbitpush_pog(g, tp->seqs->rdseqs->bits, tp->seqs->rdoffs->buffer[ridx] + tp->regs[0]->buffer[ridx], tp->regs[1]->buffer[ridx] - tp->regs[0]->buffer[ridx]);
	}
	if(0){
		print_seqs_pog(g, "p0.fa", NULL);
	}
	end_pog(g);
	g->aln_mode = POG_ALNMODE_OVERLAP;
	// finding a best break point
	{
		u2v *rs;
		u1i *s;
		u2i i, idx, bst, bsi, cnt;
		rs = init_u2v(256);
		bst = 0;
		bsi = MAX_U2;
		s = g->msa->buffer + g->msa_len * g->seqs->nseq;
		for(idx=0;idx<g->msa_len;idx++){
			if(s[idx] == 4) continue;
			cnt = 0;
			for(ridx=0;ridx<g->seqs->nseq;ridx++){
				if(g->msa->buffer[g->msa_len * ridx + idx] == s[idx]) cnt ++;
			}
			if(cnt > bst){
				bst = cnt;
				bsi = idx;
				clear_u2v(rs);
				push_u2v(rs, idx);
			} else if(cnt == bst){
				push_u2v(rs, idx);
			}
		}
		if(rs->size){
			bsi = rs->buffer[rs->size / 2];
		}
		free_u2v(rs);
		if(bsi == MAX_U2){
			return direct_run_tripog(tp);
		}
		if(cns_debug > 1){
			fprintf(stderr, "BREAKPOINT[MSA]: %u/%u\n", bsi, g->msa_len);
		}
		// transform coordinate
		i = 0;
		for(ridx=0;ridx<tp->seqs->nseq;ridx++){
			if(tp->regs[0]->buffer[ridx] == MAX_U2) continue;
			cnt = 0;
			s = g->msa->buffer + g->msa_len * i;
			for(idx=0;idx<bsi;idx++){
				if(s[idx] != 4) cnt ++;
			}
			tp->regs[0]->buffer[ridx] = tp->regs[0]->buffer[ridx] + cnt;
			tp->regs[1]->buffer[ridx] = tp->regs[0]->buffer[ridx];
			if(cns_debug > 1){
				fprintf(stderr, "BREAKPOINT[%u:%u]: %u/%u\n", i, ridx, tp->regs[0]->buffer[ridx], tp->seqs->rdlens->buffer[ridx]);
			}
			i ++;
		}
	}
	// forward cns to seqs' ends
	g = tp->pogs[2];
	beg_pog(g);
	for(ridx=0;ridx<tp->seqs->nseq;ridx++){
		if(tp->regs[0]->buffer[ridx] == MAX_U2) continue;
		fwdbitpush_pog(g, tp->seqs->rdseqs->bits, tp->seqs->rdoffs->buffer[ridx] + tp->regs[1]->buffer[ridx], tp->seqs->rdlens->buffer[ridx] - tp->regs[1]->buffer[ridx]);
	}
	if(0){
		print_seqs_pog(g, "p2.fa", NULL);
	}
	end_pog(g);
	if(g->cns->size == 0){
		return direct_run_tripog(tp);
	}
	// backward cns to seqs' begs
	g = tp->pogs[1];
	beg_pog(g);
	for(ridx=0;ridx<tp->seqs->nseq;ridx++){
		if(tp->regs[0]->buffer[ridx] == MAX_U2) continue;
		revbitpush_pog(g, tp->seqs->rdseqs->bits, tp->seqs->rdoffs->buffer[ridx], tp->regs[0]->buffer[ridx]);
	}
	if(0){
		print_seqs_pog(g, "p1.fa", NULL);
	}
	end_pog(g);
	if(g->cns->size == 0){
		return direct_run_tripog(tp);
	}
	// merge two parts
	clear_basebank(tp->cns);
	fast_revbits2basebank(tp->cns, tp->pogs[1]->cns->bits, 0, tp->pogs[1]->cns->size);
	fast_fwdbits2basebank(tp->cns, tp->pogs[2]->cns->bits, 0, tp->pogs[2]->cns->size);
	tp->is_tripog = 1;
}

#endif
