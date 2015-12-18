/******************************************************************************
 * Copyright © 2014-2015 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#define uthash_malloc(size) iguana_memalloc(mem,size,1)
#define uthash_free iguana_stub

#include "iguana777.h"
void iguana_stub(void *ptr,int size) { printf("uthash_free ptr.%p %d\n",ptr,size); }

#define iguana_hashfind(hashtable,key,keylen) iguana_hashsetPT(hashtable,0,key,keylen,-1)

struct iguana_kvitem *iguana_hashsetPT(struct iguana_kvitem *hashtable,struct iguana_memspace *mem,void *key,int32_t keylen,int32_t itemind)
{
    struct iguana_kvitem *ptr = 0; int32_t allocsize;
    HASH_FIND(hh,hashtable,key,keylen,ptr);
    if ( ptr == 0 && itemind >= 0 )
    {
        allocsize = (int32_t)(sizeof(*ptr));
        if ( mem != 0 )
            ptr = iguana_memalloc(mem,allocsize,1);
        if ( ptr == 0 )
            printf("fatal alloc error in hashset\n"), exit(-1);
        //printf("ptr.%p allocsize.%d key.%p keylen.%d itemind.%d\n",ptr,allocsize,key,keylen,itemind);
        ptr->hh.itemind = itemind;
        HASH_ADD_KEYPTR(hh,hashtable,key,keylen,ptr);
    }
    if ( ptr != 0 )
    {
        struct iguana_kvitem *tmp;
        HASH_FIND(hh,hashtable,key,keylen,tmp);
        char str[65];
        init_hexbytes_noT(str,key,keylen);
        if ( tmp != ptr )
            printf("%s itemind.%d search error %p != %p\n",str,itemind,ptr,tmp);
        // else printf("added.(%s) height.%d %p\n",str,itemind,ptr);
    }
    return(ptr);
}

struct iguana_txblock *iguana_peerramchainPT(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_txblock *origtxdata,struct iguana_msgtx *txarray,int32_t txn_count,uint8_t *data,int32_t recvlen)
{
    struct iguana_txid *T,*t; struct iguana_unspent *U,*u; struct iguana_spend *S,*s;
    bits256 *externalT; struct iguana_kvitem *txids,*pkhashes,*ptr; struct iguana_pkhash *P;
    struct iguana_memspace *txmem,*hashmem; struct iguana_msgtx *tx; struct iguana_txblock *txdata = 0;
    int32_t i,j,numvins,numvouts,numexternal,numpkinds,scriptlen,sequence;
    uint32_t txidind,unspentind,spendind,pkind; uint8_t *script,rmd160[20];
    txmem = &addr->TXDATA, hashmem = &addr->HASHMEM;
    txids = pkhashes = 0;
    //printf("recvlen.%d txn_count.%d\n",recvlen,txn_count);
    if ( (txdata= iguana_ramchainptrs(&T,&U,&S,&P,0,txmem,origtxdata)) == 0 || T == 0 || U == 0 || S == 0 || P == 0 )
    {
        printf("fatal error getting txdataptrs\n");
        exit(-1);
        return(0);
    }
    txidind = unspentind = spendind = pkind = 0;
    for (i=numvouts=numpkinds=0; i<txn_count; i++,txidind++)
    {
        tx = &txarray[i];
        t = &T[txidind];
        t->txid = tx->txid, t->txidind = txidind, t->firstvout = unspentind, t->numvouts = tx->tx_out;
        iguana_hashsetPT(txids,hashmem,t->txid.bytes,sizeof(bits256),txidind);
        for (j=0; j<tx->tx_out; j++,numvouts++,unspentind++)
        {
            u = &U[unspentind];
            script = tx->vouts[j].pk_script, scriptlen = tx->vouts[j].pk_scriptlen;
            iguana_calcrmd160(coin,rmd160,script,scriptlen,tx->txid);
            if ( (ptr= iguana_hashfind(pkhashes,rmd160,sizeof(rmd160))) == 0 )
            {
                memcpy(P[numpkinds].rmd160,rmd160,sizeof(rmd160));
                if ( (ptr= iguana_hashsetPT(pkhashes,hashmem,&P[numpkinds],sizeof(P[numpkinds].rmd160),numpkinds)) == 0 )
                    printf("fatal error adding pkhash\n"), exit(-1);
                //printf("%016lx new pkind.%d pkoffset.%d %d\n",*(long *)rmd160,numpkinds,txdata->pkoffset,(int32_t)((long)&P[numpkinds] - (long)txdata));
                numpkinds++;
            }
            u->value = tx->vouts[j].value, u->txidind = txidind;
            u->pkind = ptr->hh.itemind;
            P[u->pkind].firstunspentind = unspentind;
            // prevunspentind requires having accts, so that waits for third pass
        }
    }
    //printf("reallocP.%p -> ",P);
    if ( (txdata->numpkinds= numpkinds) > 0 )
        P = iguana_memalloc(txmem,sizeof(*P) * numpkinds,0);
    //printf("%p\n",P);
    externalT = iguana_memalloc(txmem,0,1);
    txidind = 0;
    for (i=numvins=numexternal=0; i<txn_count; i++,txidind++)
    {
        tx = &txarray[i];
        t = &T[txidind];
        t->firstvin = spendind;
        for (j=0; j<tx->tx_in; j++)
        {
            script = tx->vins[j].script, scriptlen = tx->vins[j].scriptlen;
            s = &S[spendind];
            if ( (sequence= tx->vins[j].sequence) != (uint32_t)-1 )
                s->diffsequence = 1;
            s->vout = tx->vins[j].prev_vout;
            if ( s->vout != 0xffff )
            {
                if ( (ptr= iguana_hashfind(txids,tx->vins[j].prev_hash.bytes,sizeof(bits256))) != 0 )
                {
                    if ( (s->spendtxidind= ptr->hh.itemind) >= txdata->numtxids )
                    {
                        s->external = 1;
                        s->spendtxidind -= txdata->numtxids;
                    }
                }
                else
                {
                    s->external = 1;
                    externalT[numexternal] = tx->vins[j].prev_hash;
                    iguana_hashsetPT(txids,hashmem,externalT[numexternal].bytes,sizeof(externalT[numexternal]),txdata->numtxids + numexternal);
                    s->spendtxidind = numexternal++;
                }
                spendind++;
                numvins++;
                //printf("spendind.%d\n",spendind);
            } //else printf("vout.%x\n",s->vout);
            // prevspendind requires having accts, so that waits for third pass
        }
        t->numvins = numvins;
    }
    if ( (txdata->numexternaltxids= numexternal) > 0 )
        externalT = iguana_memalloc(txmem,sizeof(*externalT) * numexternal,0);
    txdata->datalen = (int32_t)txmem->used;
    txdata->numspends = numvins;
    txdata->numpkinds = numpkinds;
    //printf("%p datalen.%d T.%d U.%d S.%d P.%d X.%d\n",txdata,txdata->datalen,txdata->numtxids,txdata->numunspents,txdata->numspends,txdata->numpkinds,txdata->numexternaltxids);
    if ( numvouts != txdata->numunspents || i != txdata->numtxids )
    {
        printf("counts mismatch: numvins %d != %d txdata->numvins || numvouts %d != %d txdata->numvouts || i %d != %d txdata->numtxids\n",numvins,txdata->numspends,numvouts,txdata->numunspents,i,txdata->numtxids);
        exit(-1);
        return(0);
    }
    {
        static int32_t maxrecvlen,maxdatalen,maxhashmem; static double recvsum,datasum;
        recvsum += recvlen, datasum += txdata->datalen;
        if ( recvlen > maxrecvlen )
            printf("[%.3f] %.0f/%.0f maxrecvlen %d -> %d\n",recvsum/datasum,recvsum,datasum,maxrecvlen,recvlen), maxrecvlen = recvlen;
        if ( txdata->datalen > maxdatalen )
            printf("[%.3f] %.0f/%.0f maxdatalen %d -> %d\n",recvsum/datasum,recvsum,datasum,maxdatalen,txdata->datalen), maxdatalen = txdata->datalen;
        if ( hashmem->used > maxhashmem )
            printf("[%.3f] %.0f/%.0f maxhashmem %d -> %ld\n",recvsum/datasum,recvsum,datasum,maxhashmem,hashmem->used), maxhashmem = (int32_t)hashmem->used;
        if ( (rand() % 10000) == 0 )
            printf("[%.3f] %.0f/%.0f recvlen vs datalen\n",recvsum/datasum,recvsum,datasum);
    }
    //printf("numpkinds.%d numspends.%d\n",txdata->numpkinds,txdata->numspends);
    return(txdata);
}

// two passes to check data size
int32_t iguana_rwvin(int32_t rwflag,struct iguana_memspace *mem,uint8_t *serialized,struct iguana_msgvin *msg)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(msg->prev_hash),msg->prev_hash.bytes);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(msg->prev_vout),&msg->prev_vout);
    //printf("vin.(%s) %d\n",bits256_str(msg->prev_hash),msg->prev_vout);
    len += iguana_rwvarint32(rwflag,&serialized[len],&msg->scriptlen);
    if ( rwflag == 0 )
        msg->script = iguana_memalloc(mem,msg->scriptlen,1);
    len += iguana_rwmem(rwflag,&serialized[len],msg->scriptlen,msg->script);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(msg->sequence),&msg->sequence);
    //int i; for (i=0; i<msg->scriptlen; i++)
    // printf("%02x ",msg->script[i]);
    //printf(" inscriptlen.%d, prevhash.%llx prev_vout.%d | ",msg->scriptlen,(long long)msg->prev_hash.txid,msg->prev_vout);
    return(len);
}

int32_t iguana_rwvout(int32_t rwflag,struct iguana_memspace *mem,uint8_t *serialized,struct iguana_msgvout *msg)
{
    int32_t len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(msg->value),&msg->value);
    len += iguana_rwvarint32(rwflag,&serialized[len],&msg->pk_scriptlen);
    if ( rwflag == 0 )
        msg->pk_script = iguana_memalloc(mem,msg->pk_scriptlen,1);
    len += iguana_rwmem(rwflag,&serialized[len],msg->pk_scriptlen,msg->pk_script);
    //printf("(%.8f scriptlen.%d) ",dstr(msg->value),msg->pk_scriptlen);
    //int i; for (i=0; i<msg->pk_scriptlen; i++)
    //    printf("%02x",msg->pk_script[i]);
    //printf("\n");
    return(len);
}

int32_t iguana_rwtx(int32_t rwflag,struct iguana_memspace *mem,uint8_t *serialized,struct iguana_msgtx *msg,int32_t maxsize,bits256 *txidp,int32_t height,int32_t hastimestamp)
{
    int32_t i,len = 0; uint8_t *txstart = serialized; char txidstr[65]; uint32_t timestamp;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(msg->version),&msg->version);
    if ( hastimestamp != 0 )
        len += iguana_rwnum(rwflag,&serialized[len],sizeof(timestamp),&timestamp);
    len += iguana_rwvarint32(rwflag,&serialized[len],&msg->tx_in);
    //printf("version.%d ",msg->version);
    if ( msg->tx_in > 0 && msg->tx_out*100 < maxsize )
    {
        if ( rwflag == 0 )
            msg->vins = iguana_memalloc(mem,msg->tx_in * sizeof(*msg->vins),1);
        for (i=0; i<msg->tx_in; i++)
            len += iguana_rwvin(rwflag,mem,&serialized[len],&msg->vins[i]);
        //printf("numvins.%d\n",msg->tx_in);
    }
    else
    {
        printf("invalid tx_in.%d\n",msg->tx_in);
        return(-1);
    }
    len += iguana_rwvarint32(rwflag,&serialized[len],&msg->tx_out);
    if ( msg->tx_out > 0 && msg->tx_out*32 < maxsize )
    {
        //printf("numvouts.%d ",msg->tx_out);
        if ( rwflag == 0 )
            msg->vouts = iguana_memalloc(mem,msg->tx_out * sizeof(*msg->vouts),1);
        for (i=0; i<msg->tx_out; i++)
            len += iguana_rwvout(rwflag,mem,&serialized[len],&msg->vouts[i]);
    }
    else
    {
        printf("invalid tx_out.%d\n",msg->tx_out);
        return(-1);
    }
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(msg->lock_time),&msg->lock_time);
    *txidp = bits256_doublesha256(txidstr,txstart,len);
    msg->allocsize = len;
    return(len);
}

int32_t iguana_gentxarray(struct iguana_info *coin,struct iguana_memspace *mem,struct iguana_txblock *txdata,int32_t *lenp,uint8_t *data,int32_t datalen)
{
    struct iguana_msgtx *tx; bits256 hash2; struct iguana_msgblock msg; int32_t i,n,len,numvouts,numvins;
    memset(&msg,0,sizeof(msg));
    len = iguana_rwblock(0,&hash2,data,&msg);
    iguana_convblock(&txdata->block,&msg,hash2,-1);
    tx = iguana_memalloc(mem,msg.txn_count*sizeof(*tx),1);
    for (i=numvins=numvouts=0; i<msg.txn_count; i++)
    {
        if ( (n= iguana_rwtx(0,mem,&data[len],&tx[i],datalen - len,&tx[i].txid,txdata->block.height,coin->chain->hastimestamp)) < 0 )
            break;
        numvouts += tx[i].tx_out;
        numvins += tx[i].tx_in;
        len += n;
    }
    if ( coin->chain->hastimestamp != 0 && len != datalen && data[len] == (datalen - len - 1) )
    {
        //printf("\n>>>>>>>>>>> len.%d vs datalen.%d [%d]\n",len,datalen,data[len]);
        memcpy(txdata->space,&data[len],datalen-len);
        len += (datalen-len);
        txdata->extralen = (datalen - len);
    } else txdata->extralen = 0;
    txdata->recvlen = len;
    txdata->numtxids = msg.txn_count;
    txdata->numunspents = numvouts;
    txdata->numspends = numvins;
    return(len);
}

struct iguana_bundlereq *iguana_bundlereq(struct iguana_info *coin,struct iguana_peer *addr,int32_t type,int32_t datalen)
{
    struct iguana_bundlereq *req; int32_t allocsize;
    allocsize = (uint32_t)sizeof(*req) + datalen;
    req = mycalloc(type,1,allocsize);
    req->allocsize = allocsize;
    req->datalen = datalen;
    req->addr = addr;
    req->coin = coin;
    req->type = type;
    return(req);
}

void iguana_gottxidsM(struct iguana_info *coin,struct iguana_peer *addr,bits256 *txids,int32_t n)
{
    struct iguana_bundlereq *req;
    printf("got %d txids from %s\n",n,addr->ipaddr);
    req = iguana_bundlereq(coin,addr,'T',0);
    req->hashes = txids, req->n = n;
    queue_enqueue("bundlesQ",&coin->bundlesQ,&req->DL,0);
}

void iguana_gotunconfirmedM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_msgtx *tx,uint8_t *data,int32_t datalen)
{
    struct iguana_bundlereq *req;
    char str[65]; bits256_str(str,tx->txid);
    printf("%s unconfirmed.%s\n",addr->ipaddr,str);
    req = iguana_bundlereq(coin,addr,'U',datalen);
    req->datalen = datalen;
    memcpy(req->serialized,data,datalen);
    //iguana_freetx(tx,1);
    queue_enqueue("bundlesQ",&coin->bundlesQ,&req->DL,0);
}

void iguana_gotheadersM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_block *blocks,int32_t n)
{
    struct iguana_bundlereq *req;
    if ( addr != 0 )
    {
        addr->recvhdrs++;
        if ( addr->pendhdrs > 0 )
            addr->pendhdrs--;
        //printf("%s blocks[%d] ht.%d gotheaders pend.%d %.0f\n",addr->ipaddr,n,blocks[0].height,addr->pendhdrs,milliseconds());
    }
    req = iguana_bundlereq(coin,addr,'H',0);
    req->blocks = blocks, req->n = n;
    queue_enqueue("bundlesQ",&coin->bundlesQ,&req->DL,0);
}

void iguana_gotblockhashesM(struct iguana_info *coin,struct iguana_peer *addr,bits256 *blockhashes,int32_t n)
{
    struct iguana_bundlereq *req;
    if ( addr != 0 )
    {
        addr->recvhdrs++;
        if ( addr->pendhdrs > 0 )
            addr->pendhdrs--;
    }
    req = iguana_bundlereq(coin,addr,'S',0);
    req->hashes = blockhashes, req->n = n;
    //printf("bundlesQ blockhashes.%p[%d]\n",blockhashes,n);
    queue_enqueue("bundlesQ",&coin->bundlesQ,&req->DL,0);
}

void iguana_gotblockM(struct iguana_info *coin,struct iguana_peer *addr,struct iguana_txblock *txdata,struct iguana_msgtx *txarray,uint8_t *data,int32_t recvlen)
{
    struct iguana_bundlereq *req; int32_t i,z,fpos,bundlei; FILE *fp; char fname[1024];
    if ( 0 )
    {
        for (i=0; i<txdata->space[0]; i++)
            if ( txdata->space[i] != 0 )
                break;
        if ( i != txdata->space[0] )
        {
            for (i=0; i<txdata->space[0]; i++)
                printf("%02x ",txdata->space[i]);
            printf("extra\n");
        }
    }
    req = iguana_bundlereq(coin,addr,'B',0);
    if ( addr != 0 )
    {
        if ( addr->pendblocks > 0 )
            addr->pendblocks--;
        addr->lastblockrecv = (uint32_t)time(NULL);
        addr->recvblocks += 1.;
        addr->recvtotal += recvlen;
        if ( (txdata= iguana_peerramchainPT(coin,addr,txdata,txarray,txdata->block.txn_count,data,recvlen)) != 0 )
        {
            //fpos = (addr->fp != 0) ? ftell(addr->fp) : 0;
            //txdatabits = iguana_calctxidbits(addr->addrind,addr->filecount,(uint32_t)fpos,txdata->datalen);
            //txdatabits = iguana_peerfilePT(coin,addr,txdata->block.hash2,txdatabits,txdata->datalen);
            fpos = 0;
            if ( (bundlei= iguana_peerfname(coin,fname,addr->ipbits,txdata->block.hash2)) < 0 )
            {
                if ( (fp= fopen(fname,"wb")) != 0 )
                    coin->peers.numfiles++;
            }
            else
            {
                if ( (fp= fopen(fname,"rb+")) == 0 )
                {
                    if ( (fp= fopen(fname,"wb")) != 0 )
                    {
                        z = -1;
                        coin->peers.numfiles++;
                        for (i=0; i<coin->chain->bundlesize; i++)
                            fwrite(&z,1,sizeof(z),fp);
                        fclose(fp);
                        fp = fopen(fname,"rb+");
                    }
                }
                if ( fp != 0 )
                {
                    fseek(fp,0,SEEK_END);
                    fpos = (int32_t)ftell(fp);
                }
            }
            if ( fp != 0 )
            {
                //printf("fpos.%d: bundlei.%d datalen.%d\n",fpos,bundlei,txdata->datalen);
                fwrite(&bundlei,1,sizeof(bundlei),fp);
                fwrite(&txdata->block.hash2,1,sizeof(txdata->block.hash2),fp);
                fwrite(&txdata->datalen,1,sizeof(txdata->datalen),fp);
                fwrite(txdata,1,txdata->datalen,fp);
                if ( bundlei >= 0 && bundlei < coin->chain->bundlesize )
                {
                    fseek(fp,bundlei * sizeof(bundlei),SEEK_SET);
                    //printf("bundlei[%d] <- fpos.%d\n",bundlei,fpos);
                    fwrite(&fpos,1,sizeof(fpos),fp);
                } else printf("error with bundlei.%d vs %d\n",bundlei,coin->chain->bundlesize);
                fclose(fp);
                //for (i=0; i<txdata->numpkinds; i++)
                //    printf("%016lx ",*(long *)((struct iguana_pkhash *)((long)txdata + txdata->pkoffset))[i].rmd160);
                //printf("datalen.%d T.%d U.%d S.%d P.%d X.%d\n",txdata->datalen,txdata->numtxids,txdata->numunspents,txdata->numspends,txdata->numpkinds,txdata->numexternaltxids);
                //printf("create.(%s) %d\n",fname,coin->peers.numfiles);
            }
            req->datalen = txdata->datalen;
        }
    }
    coin->recvcount++;
    coin->recvtime = (uint32_t)time(NULL);
    req->block = txdata->block;
    req->addr = addr;
    req->block.txn_count = req->numtx = txdata->block.txn_count;
    queue_enqueue("bundlesQ",&coin->bundlesQ,&req->DL,0);
}

struct iguana_txblock *iguana_peertxdata(struct iguana_info *coin,char *fname,struct iguana_memspace *mem,uint32_t ipbits,bits256 hash2)
{
    int32_t bundlei,datalen,checki,fpos; char str[65],str2[65]; FILE *fp;
    bits256 checkhash2; struct iguana_txblock *txdata = 0;
    if ( (bundlei= iguana_peerfname(coin,fname,ipbits,hash2)) >= 0 )
    {
        if ( (fp= fopen(fname,"rb")) != 0 )
        {
            fseek(fp,bundlei * sizeof(bundlei),SEEK_SET);
            fread(&fpos,1,sizeof(fpos),fp);
            fseek(fp,fpos,SEEK_SET);
            fread(&checki,1,sizeof(checki),fp);
            if ( ftell(fp)-sizeof(checki) == fpos && bundlei == checki )
            {
                fread(&checkhash2,1,sizeof(checkhash2),fp);
                if ( memcmp(hash2.bytes,checkhash2.bytes,sizeof(hash2)) == 0 )
                {
                    fread(&datalen,1,sizeof(datalen),fp);
                    if ( datalen < (mem->totalsize - mem->used - 4) )
                    {
                        if ( (txdata= iguana_memalloc(mem,datalen,0)) != 0 )
                        {
                            fread(txdata,1,datalen,fp);
                            if ( txdata->datalen != datalen )
                            {
                                printf("peertxdata txdata->datalen.%d != %d\n",txdata->datalen,datalen);
                                txdata = 0;
                                iguana_memreset(mem);
                            } //else printf("SUCCESS txdata.%p\n",txdata);
                        } else printf("peertxdata error allocating txdata\n");
                    } else printf("peertxdata datalen %d vs %ld\n",datalen,mem->totalsize - mem->used - 4);
                } else printf("peertxdata hash mismatch %s != %s\n",bits256_str(str,hash2),bits256_str(str2,checkhash2));
            } else printf("peertxdata bundlei.%d != checki.%d, fpos.%d ftell.%ld\n",bundlei,checki,fpos,ftell(fp));
            fclose(fp);
        } else printf("cant find file\n");
    } else printf("bundlei.%d\n",bundlei);
    return(txdata);
}

