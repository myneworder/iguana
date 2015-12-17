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

#include "iguana777.h"

int32_t iguana_peerfname(struct iguana_info *coin,char *fname,uint32_t ipbits,bits256 hash2)
{
    struct iguana_bundle *bp; int32_t bundlei; char str[65];
    if ( ipbits == 0 )
        printf("illegal ipbits.%d\n",ipbits), getchar();
    if ( (bp= iguana_bundlesearch(coin,&bundlei,hash2)) != 0 )
        hash2 = bp->bundlehash2;
    sprintf(fname,"tmp/%s/%s.peer%08x",coin->symbol,bits256_str(str,hash2),ipbits);
    return(bundlei);
}

struct iguana_ramchain *iguana_bundlemergeHT(struct iguana_info *coin,struct iguana_memspace *mem,struct iguana_txblock *ptrs[],int32_t n,struct iguana_bundle *bp)
{
    int32_t i; struct iguana_ramchain *ramchain=0; struct iguana_block *block;
    if ( ptrs[0] != 0 && (block= bp->blocks[0]) != 0 && (ramchain= iguana_ramchaininit(coin,mem,ptrs[0],bp->prevbundlehash2,block->prev_block,block->hash2,0,ptrs[0]->datalen)) != 0 )
    {
        for (i=1; i<n; i++)
        {
            if ( ptrs[i] != 0 && (block= bp->blocks[i]) != 0 )
            {
                if ( iguana_ramchainmerge(coin,mem,ramchain,ptrs[i]) < 0 )
                {
                    printf("error merging ramchain.%s hdrsi.%d at ptrs[%d]\n",coin->symbol,bp->hdrsi,i);
                    iguana_ramchainfree(coin,ramchain);
                    return(0);
                }
            }
            else
            {
                printf("error generating ramchain.%s hdrsi.%d for ptrs[%d]\n",coin->symbol,bp->hdrsi,i);
                iguana_ramchainfree(coin,ramchain);
                return(0);
            }
        }
    }
    return(ramchain);
}

int32_t iguana_peerfile_exists(struct iguana_info *coin,struct iguana_peer *addr,char *fname,bits256 hash2)
{
    FILE *fp; int32_t bundlei;
    if ( (bundlei= iguana_peerfname(coin,fname,addr->ipbits,hash2)) >= 0 )
    {
        if ( (fp= fopen(fname,"rb")) == 0 )
            bundlei = -1;
        else fclose(fp);
    }
    return(bundlei);
}

int32_t iguana_bundlesaveHT(struct iguana_info *coin,struct iguana_memspace *mem,struct iguana_memspace *memB,struct iguana_bundle *bp) // helper thread
{
    struct iguana_txblock *ptrs[IGUANA_MAXBUNDLESIZE]; struct iguana_block *block;
    char fname[1024]; uint64_t estimatedsize = 0;
    int32_t i,maxrecv,addrind,flag,numdirs=0; struct iguana_ramchain *ramchain;
    flag = maxrecv = 0;
    for (i=0; i<bp->n && i<coin->chain->bundlesize; i++)
    {
        if ( (block= bp->blocks[i]) != 0 )
        {
            if ( memcmp(block->hash2.bytes,coin->chain->genesis_hashdata,sizeof(bits256)) == 0 )
                ptrs[i] = (struct iguana_txblock *)coin->chain->genesis_hashdata, flag++;
            else
            {
                iguana_meminit(&memB[i],"ramchainB",0,block->recvlen + 4096,0);
                if ( (ptrs[i]= iguana_peertxdata(coin,fname,&memB[i],block->ipbits,block->hash2)) != 0 )
                {
                    if ( block->recvlen > maxrecv )
                        maxrecv = block->recvlen;
                    estimatedsize += block->recvlen;
                    flag++;
                }
                else
                {
                    printf("error (%s) hdrs.%d ptr[%d]\n",fname,bp->hdrsi,i);
                    CLEARBIT(bp->recv,i);
                    bp->issued[i] = 0;
                    block = 0;
                }
            }
        }
    }
    if ( flag == i )
    {
        iguana_meminit(mem,"bundleHT",0,estimatedsize + IGUANA_MAXPACKETSIZE,0);
        printf(">>>>>>>>> start MERGE.(%ld) i.%d flag.%d estimated.%ld maxrecv.%d\n",(long)mem->totalsize,i,flag,(long)estimatedsize,maxrecv);
        if ( (ramchain= iguana_bundlemergeHT(coin,mem,ptrs,i,bp)) != 0 )
        {
            iguana_ramchainsave(coin,ramchain);
            iguana_ramchainfree(coin,ramchain);
            printf("ramchain saved\n");
            bp->emitfinish = (uint32_t)time(NULL);
         } else bp->emitfinish = 0;
        iguana_mempurge(mem);
        for (addrind=0; addrind<IGUANA_MAXPEERS; addrind++)
        {
            if ( coin->peers.active[addrind].ipbits != 0 )
            {
                if ( iguana_peerfile_exists(coin,&coin->peers.active[addrind],fname,bp->bundlehash2) >= 0 )
                {
                    //printf("remove.(%s)\n",fname);
                    iguana_removefile(fname,0);
                    coin->peers.numfiles--;
                }
            }
        }
    }
    else
    {
        printf(">>>>> bundlesaveHT error: numdirs.%d i.%d flag.%d\n",numdirs,i,flag);
        bp->emitfinish = 0;
    }
    for (i=0; i<bp->n && i<coin->chain->bundlesize; i++)
        iguana_mempurge(&memB[i]);
    return(flag);
}

void iguana_emitQ(struct iguana_info *coin,struct iguana_bundle *bp)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->bp = bp, ptr->hdrsi = bp->hdrsi;
    ptr->type = 'E';
    printf("%s EMIT.%d[%d] emitfinish.%u\n",coin->symbol,ptr->hdrsi,bp->n,bp->emitfinish);
    queue_enqueue("helperQ",&helperQ,&ptr->DL,0);
}

/*void iguana_txdataQ(struct iguana_info *coin,struct iguana_peer *addr,FILE *fp,long fpos,int32_t datalen)
{
    struct iguana_helper *ptr;
    ptr = mycalloc('i',1,sizeof(*ptr));
    ptr->allocsize = sizeof(*ptr);
    ptr->coin = coin;
    ptr->addr = addr, ptr->fp = fp, ptr->fpos = fpos, ptr->datalen = datalen;
    ptr->type = 'T';
    queue_enqueue("helperQ",&helperQ,&ptr->DL,0);
}*/

void iguana_flushQ(struct iguana_info *coin,struct iguana_peer *addr)
{
    struct iguana_helper *ptr;
    if ( time(NULL) > addr->lastflush+3 )
    {
        ptr = mycalloc('i',1,sizeof(*ptr));
        ptr->allocsize = sizeof(*ptr);
        ptr->coin = coin;
        ptr->addr = addr;
        ptr->type = 'F';
        //printf("FLUSH.%s %u lag.%d\n",addr->ipaddr,addr->lastflush,(int32_t)(time(NULL)-addr->lastflush));
        addr->lastflush = (uint32_t)time(NULL);
        queue_enqueue("helperQ",&helperQ,&ptr->DL,0);
    }
}

int32_t iguana_helpertask(FILE *fp,struct iguana_memspace *mem,struct iguana_memspace *memB,struct iguana_helper *ptr)
{
    struct iguana_info *coin; struct iguana_peer *addr; struct iguana_bundle *bp;
    coin = ptr->coin, addr = ptr->addr;
    if ( ptr->type == 'F' )
    {
        if ( addr != 0 && addr->fp != 0 )
        {
            //printf("flush.%s %p\n",addr->ipaddr,addr->fp);
            fflush(addr->fp);
        }
    }
    else if ( ptr->type == 'E' )
    {
        printf("emitQ coin.%p bp.%p\n",ptr->coin,ptr->bp);
        if ( (coin= ptr->coin) != 0 )
        {
            if ( (bp= ptr->bp) != 0 )
            {
                bp->emitfinish = (uint32_t)time(NULL);
                if ( iguana_bundlesaveHT(coin,mem,memB,bp) == 0 )
                    coin->numemitted++;
            }
            printf("MAXBUNDLES.%d vs max.%d estsize %ld vs cache.%ld\n",coin->MAXBUNDLES,_IGUANA_MAXBUNDLES,(long)coin->estsize,(long)coin->MAXRECVCACHE);
            if ( coin->MAXBUNDLES > IGUANA_MAXACTIVEBUNDLES || (coin->estsize > coin->MAXRECVCACHE*.9 && coin->MAXBUNDLES > _IGUANA_MAXBUNDLES) )
                coin->MAXBUNDLES--;
            else if ( (coin->MAXBUNDLES * coin->estsize)/(coin->activebundles+1) < coin->MAXRECVCACHE*.75 )
                coin->MAXBUNDLES += (coin->MAXBUNDLES >> 2) + 1;
            else printf("no change to MAXBUNDLES.%d\n",coin->MAXBUNDLES);
        } else printf("no coin in helper request?\n");
    }
    return(0);
}

void iguana_helper(void *arg)
{
    FILE *fp = 0; char fname[512],name[64],*helpername = 0; cJSON *argjson=0; int32_t i,flag;
    struct iguana_helper *ptr; struct iguana_info *coin; struct iguana_memspace MEM,*MEMB;
    if ( arg != 0 && (argjson= cJSON_Parse(arg)) != 0 )
        helpername = jstr(argjson,"name");
    if ( helpername == 0 )
    {
        sprintf(name,"helper.%d",rand());
        helpername = name;
    }
    sprintf(fname,"tmp/%s",helpername);
    fp = fopen(fname,"wb");
    if ( argjson != 0 )
        free_json(argjson);
    memset(&MEM,0,sizeof(MEM));
    MEMB = mycalloc('b',IGUANA_MAXBUNDLESIZE,sizeof(*MEMB));
    while ( 1 )
    {
        flag = 0;
        while ( (ptr= queue_dequeue(&helperQ,0)) != 0 )
        {
            iguana_helpertask(fp,&MEM,MEMB,ptr);
            myfree(ptr,ptr->allocsize);
            flag++;
        }
        if ( flag == 0 )
        {
            for (i=0; i<sizeof(Coins)/sizeof(*Coins); i++)
            {
                coin = &Coins[i];
                if ( coin->launched != 0 )
                    flag += iguana_rpctest(coin);
            }
            if ( flag == 0 )
                usleep(10000);
        }
    }
}

