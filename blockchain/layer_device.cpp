#include "layer_device.h"

extern uint32 g_devicenum[2];//设备个数
extern uint32 g_devicerange;//设备坐标范围
extern uint32 g_devicestep;//设备步进值
extern uint32 g_dealnumber;//交易原子列表个数
extern uint32 g_dealindex;//交易原子列表索引
extern deal_t *g_deal;//交易原子列表
extern CRITICAL_SECTION g_cs;
extern device_t *g_device;//设备数组
extern mainchain_t g_mainchain;//主链
extern volatile uint32 g_index;//临时用来统计交易号码的(以后会用hash_t代替,计数从1开始)

//STEP_CONNECT
void connect_recv(device_t *device)
{
	//queue->route
	uint32 i;
	uint8 flag;
	route_t *route;
	queue_t *queue,*prev;
	index_t index;

	queue=device->queue;
	while(queue)
	{
		if (queue->step==STEP_CONNECT)
		{
			//queue process
			index.number=*(uint32 *)queue->data;
			index.index=(uint32 *)(queue->data+1*sizeof(uint32));
			index.key=queue->data+(1+index.number)*sizeof(uint32);
			index.token=(uint32 *)(queue->data+(1+index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN));
			index.node=queue->data+(1+2*index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN);
			for (i=0;i<index.number;i++)
			{
				if (index.index[i]==device->device_index)
					continue;
				flag=0;
				route=device->route;
				while(route)
				{
					if (route->device_index==index.index[i])
					{
						flag=1;
						break;
					}
					route=route->next;
				}
				if (!flag)
				{
					route=new route_t;
					route->flag=0;
					route->device_index=index.index[i];
					memcpy(route->key.e,&index.key[i*(KEY_E+KEY_LEN)],KEY_E);
					memcpy(route->key.n,&index.key[i*(KEY_E+KEY_LEN)+KEY_E],KEY_LEN);
					route->token=index.token[i];
					route->node=index.node[i];
					route->next=NULL;
					route_insert(device,route);
				}
			}
			//queue delete
			if (queue==device->queue)
			{
				device->queue=queue->next;
				if (queue->data)
				{
					delete[] queue->data;
					queue->data=NULL;
				}
				delete queue;
				queue=device->queue;
			}
			else
			{
				prev->next=queue->next;
				if (queue->data)
				{
					delete[] queue->data;
					queue->data=NULL;
				}
				delete queue;
				queue=prev->next;
			}
		}
		else
		{
			prev=queue;
			queue=queue->next;
		}
	}
}

void connect_seek(device_t *device)
{
	//broadcasting to seek/malloc device->route
	uint32 i;
	uint8 flag;
	route_t *route;

	for (i=0;i<g_devicenum[0]+g_devicenum[1];i++)
	{
		if (i==device->device_index)
			continue;
		if (math_distance(device->x,device->y,g_device[i].x,g_device[i].y)<=MAX_METRIC)
		{
			flag=0;
			route=device->route;
			while(route)
			{
				if (route->device_index==g_device[i].device_index)
				{
					flag=1;
					break;
				}
				route=route->next;
			}
			if (!flag)
			{
				route=new route_t;
				route->flag=0;
				route->device_index=g_device[i].device_index;
				memcpy(route->key.e,g_device[i].rsa.e,KEY_E);
				memcpy(route->key.n,g_device[i].rsa.n,KEY_LEN);
				route->token=g_device[i].token;
				route->node=g_device[i].node;
				route->next=NULL;
				route_insert(device,route);
			}
		}
	}
}

void connect_send(device_t *device)
{
	//route->queue(device/mainchain)
	uint8 flag;
	route_t *route;
	queue_t *queue;
	index_t index;

	//compute number(should handling)
	flag=0;
	index.number=1;
	route=device->route;
	while(route)
	{
		if (!route->flag)
		{
			route->flag=1;
			flag=1;
		}
		index.number++;
		route=route->next;
	}
	if (!flag)
		return;
	index.index=new uint32[index.number];
	index.key=new uint8[index.number*(KEY_E+KEY_LEN)];
	index.token=new uint32[index.number];
	index.node=new uint8[index.number];
	index.number=0;
	index.index[index.number]=device->device_index;
	memcpy(&index.key[index.number*(KEY_E+KEY_LEN)],device->rsa.e,KEY_E);
	memcpy(&index.key[index.number*(KEY_E+KEY_LEN)+KEY_E],device->rsa.n,KEY_LEN);
	index.token[index.number]=device->token;
	index.node[index.number]=device->node;
	index.number++;
	route=device->route;
	while(route)
	{
		index.index[index.number]=route->device_index;
		memcpy(&index.key[index.number*(KEY_E+KEY_LEN)],route->key.e,KEY_E);
		memcpy(&index.key[index.number*(KEY_E+KEY_LEN)+KEY_E],route->key.n,KEY_LEN);
		index.token[index.number]=route->token;
		index.node[index.number]=route->node;
		index.number++;
		route=route->next;
	}
	//fill route
	route=device->route;
	while(route)
	{
		queue=new queue_t;
		queue->step=STEP_CONNECT;
		queue->data=new uint8[(1+2*index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN)+index.number*sizeof(uint8)];
		*(uint32 *)queue->data=index.number;//align problem?
		memcpy(queue->data+1*sizeof(uint32),index.index,index.number*sizeof(uint32));
		memcpy(queue->data+(1+index.number)*sizeof(uint32),index.key,index.number*(KEY_E+KEY_LEN));
		memcpy(queue->data+(1+index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN),index.token,index.number*sizeof(uint32));
		memcpy(queue->data+(1+2*index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN),index.node,index.number*sizeof(uint8));
		queue_insert(&g_device[route->device_index],queue);
		route=route->next;
	}
	//fill mainchain
	queue=new queue_t;
	queue->step=STEP_CONNECT;
	queue->data=new uint8[(1+2*index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN)+index.number*sizeof(uint8)];
	*(uint32 *)queue->data=index.number;//align problem?
	memcpy(queue->data+1*sizeof(uint32),index.index,index.number*sizeof(uint32));
	memcpy(queue->data+(1+index.number)*sizeof(uint32),index.key,index.number*(KEY_E+KEY_LEN));
	memcpy(queue->data+(1+index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN),index.token,index.number*sizeof(uint32));
	memcpy(queue->data+(1+2*index.number)*sizeof(uint32)+index.number*(KEY_E+KEY_LEN),index.node,index.number*sizeof(uint8));
	queue_insert(&g_mainchain,queue);
	//release
	delete[] index.index;
	delete[] index.key;
	//update queue
	queue=new queue_t;
	queue->step=STEP_TRANSACTION;
	queue->data=NULL;
	queue_insert(device,queue);
}

//STEP_TRANSACTION
uint8 transaction_seek(transaction_t *trunk,transaction_t *branch,device_t *device)
{
	//search 2 tips(random algorithm):0-dag为空,1-dag非空.原则上需要使用手动构造出较宽的tangle(判断tip),这里我使用自动构造tangle(判断solid).
	uint32 i,j,k;
	transaction_t *transaction;

	if (!device->dag)//no genesis
		return 1;
	i=compute_tip(device->dag);
	if (!i)//no correct tip or no genesis
		return 1;
	if (i==1)
	{
		j=0;
		k=0;
	}
	else
	{
		//find tip's index
		while(1)
		{
			j=rand()%i;
			k=rand()%i;
			if (j!=k)
				break;
		}
	}
	i=0;
	transaction=device->dag;
	while(transaction)
	{
		if (!transaction->flag)//正确的tip
		{
			if (j==i)
				trunk=transaction;
			if (k==i)
				branch=transaction;
			i++;
		}
		transaction=transaction->next;
	}

	return 0;
}

uint8 transaction_verify(device_t *device,transaction_t *transaction)
{
	//交易验证：使用rsa公钥验签验证交易地址，验证交易账本。0-正确,1-交易地址错误,2-交易账本错误
	uint32 i;
	rsa_t rsa;
	route_t *route;
	uint8 result[KEY_LEN];

	//get device
	rsa.le=KEY_E;
	rsa.len=KEY_LEN;
	route=device->route;
	while(route)
	{
		if (route->device_index==transaction->deal.device_index[0])
			break;
		route=route->next;
	}
	//地址验证
	memcpy(rsa.e,route->key.e,KEY_E);
	memcpy(rsa.n,route->key.n,KEY_LEN);
	i=rsa_enc(result,transaction->cipher,rsa.len,&rsa);
	memset(&result[i],0,rsa.len-i);
	if (memcmp(result,transaction->plain,rsa.len))
		return STATUS_DEVICE;
	//账本验证
	if (transaction->deal.token>route->token)
		return STATUS_LEDGER;

	return STATUS_DONE;
}

uint32 transaction_pow(transaction_t *transaction)
{
	//通过sha256计算hash pow
	uint64 i;
	uint32 length;
	crypt_sha256 *sha256;
	uint8 content[KEY_LEN+1],result[HASH_LEN];

	length=KEY_LEN;
	memcpy(content,transaction->plain,length);
	sha256=new crypt_sha256;
	for (i=1;i<0x100000000;i++)
	{
		length=_add(content,content,1,length);
		sha256->sha256_init();
		sha256->sha256_update(content,length);
		sha256->sha256_final(result);
		if (_bitlen(result,HASH_LEN)<=HASH_LEN*8-COMPARE_LEN)
			break;
	}
	if (i==0x100000000)
		i=0;
	delete sha256;

	return (uint32)i;
}

void transaction_recv(device_t *device)
{
	//queue->delete queue
	transaction_t *trunk,*branch,*transaction;
	uint32 pow[2];
	queue_t *queue,*prev,*insert;

	queue=device->queue;
	while(queue)
	{
		if (queue->step==STEP_TRANSACTION)
		{
			//queue process
			do
			{
				if (queue->data && device->node==NODE_HEAVY)//若是重节点且有交易内容
				{
					switch(*(uint32 *)queue->data)
					do
					{
						//从tip队列中获取两笔tip交易
						flag=transaction_seek(trunk,branch,device);
						if (!flag)
						{
							//地址验证+账本验证
							flag=transaction_verify(device,trunk);
							if (flag)
							{
								trunk->flag=1;
								insert=new queue_t;
								insert->step=STEP_STATUS;
								insert->data=new uint8[sizeof(status_t)];
								*(uint32 *)insert->data=flag;
								*(uint32 *)(insert->data+1*sizeof(uint32))=trunk->index;
								queue_insert(&g_mainchain,insert);
								continue;
							}
							flag=transaction_verify(device,branch);
							if (flag)
							{
								branch->flag=1;
								insert=new queue_t;
								insert->step=STEP_STATUS;
								insert->data=new uint8[sizeof(status_t)];
								*(uint32 *)insert->data=flag;
								*(uint32 *)(insert->data+1*sizeof(uint32))=branch->index;
								queue_insert(&g_mainchain,insert);
								continue;
							}
							//pow值计算
							pow[0]=transaction_pow(trunk);
							pow[1]=transaction_pow(branch);
						}
						else
							pow[0]=pow[1]=0;
					}while(0);
					//交易
				}
			}while(0);


			//queue delete
			if (queue==device->queue)
			{
				device->queue=queue->next;
				if (queue->data)
				{
					delete[] queue->data;
					queue->data=NULL;
				}
				delete queue;
				queue=device->queue;
			}
			else
			{
				prev->next=queue->next;
				if (queue->data)
				{
					delete[] queue->data;
					queue->data=NULL;
				}
				delete queue;
				queue=prev->next;
			}
		}
		else
		{
			prev=queue;
			queue=queue->next;
		}
	}
}

void transaction_signature(transaction_t *transaction,device_t *device)
{
	//encrypt transaction with rsa private key
	uint32 i;

	i=rsa_dec(transaction->cipher,transaction->plain,device->rsa.len,&device->rsa,RSA_CRT);
	memset(&transaction->cipher[i],0,device->rsa.len-i);
}

transaction_t *transaction_generate(device_t *device)
{
	//generate transaction
	uint32 i;
	transaction_t *transaction;

	if (device->device_index!=g_deal[g_dealindex].device_index[0])//非当前笔交易索引
		return NULL;
	if (g_deal[g_dealindex].token>device->token)//账本验证
		return NULL;
	transaction=new transaction_t;
	transaction->index=++g_index;
	memcpy(&transaction->deal,&g_deal[g_dealindex],sizeof(transaction_t));
	_rand(transaction->plain,KEY_LEN);
	i=_mod(transaction->plain,transaction->plain,&device->rsa.n,KEY_LEN,KEY_LEN);
	memset(&transaction->plain[i],0,KEY_LEN-i);
	transaction_signature(transaction,device);
	transaction->transaction=TRANSACTION_NONE;
	transaction->flag=0;//给寻找和计算使用
	g_dealindex++;

	return transaction;
}

void transaction_send(device_t *device,transaction_t *transaction)
{
	//transaction->queue
	route_t *route;
	queue_t *queue;

	//update queue(self)
	if (!transaction || device->node==NODE_LIGHT)
	{
		queue=new queue_t;
		queue->step=STEP_TRANSACTION;
		queue->data=NULL;
		queue_insert(device,queue);
		return;
	}
	if (device->node==NODE_LIGHT)
	{
		route=device->route;
		while(route)
		{
			if (route->node==NODE_HEAVY)
				break;
			route=route->next;
		}
	}
	//update queue(other)
	queue=new queue_t;
	queue->step=STEP_TRANSACTION;
	queue->data=new uint8[sizeof(spv_t)];
	*(uint32 *)queue->data=TRANSACTION_NONE;
	*(uint32 *)(queue->data+1*sizeof(uint32))=transaction->index;
	memcpy(queue->data+2*sizeof(uint32),&transaction->deal,sizeof(deal_t));
	memcpy(queue->data+2*sizeof(uint32)+sizeof(deal_t),transaction->plain,KEY_LEN);
	memcpy(queue->data+2*sizeof(uint32)+sizeof(deal_t)+KEY_LEN,transaction->cipher,KEY_LEN);
	memset(queue->data+2*sizeof(uint32)+sizeof(deal_t)+2*KEY_LEN,0,2*sizeof(uint32));
	*(uint32 *)(queue->data+4*sizeof(uint32)+sizeof(deal_t)+2*KEY_LEN)=0;
	*(uint32 *)(queue->data+5*sizeof(uint32)+sizeof(deal_t)+2*KEY_LEN)=0;
	if (device->node==NODE_LIGHT)//若是轻节点,则将交易传给第一重节点
	{
		if (route)
			queue_insert(&g_device[route->device_index],queue);
	}
	else//若是重节点,则更新给自己
		queue_insert(device,queue);
}

//STEP_TANGLE
void tangle_recv(device_t *device)
{
}

//STEP_MOVE
void move_location(device_t *device)
{
	if (rand()%2)
	{
		device->x+=g_devicestep;
		device->x=math_min(device->x,g_devicerange-1);
	}
	else
	{
		device->x-=g_devicestep;
		device->x=math_max(device->x,(uint32)0);
	}
	if (rand()%2)
	{
		device->y+=g_devicestep;
		device->y=math_min(device->y,g_devicerange-1);
	}
	else
	{
		device->y-=g_devicestep;
		device->y=math_max(device->y,(uint32)0);
	}
}

void process_device(device_t *device)
{
	transaction_t *transaction;

	if (!device->queue)
		return;
	switch(device->queue->step)
	{
	case STEP_CONNECT:
		//recv
		connect_recv(device);//recv & process device's queue->route
		//process
		connect_seek(device);//search around nearby->route
		//send
		connect_send(device);//pack & send device's route->queue
		break;
	case STEP_TRANSACTION:
		//recv
		transaction_recv(device);//recv & process device's queue
		//process
		transaction=transaction_generate(device);
		//send
		transaction_send(device,transaction);//pack & send device's route->queue


#if 0
		flag=transaction_seek(trunk,branch,device);
		if (!flag)
		{
			flag=transaction_verify(device,&device->tangle[trunk]);
			if (flag)
			{
				LeaveCriticalSection(&g_cs);
				break;
			}
			flag=transaction_verify(device,&device->tangle[branch]);
			if (flag)
			{
				LeaveCriticalSection(&g_cs);
				break;
			}
			pow[0]=transaction_pow(device,&device->tangle[trunk]);
			pow[1]=transaction_pow(device,&device->tangle[branch]);
		}
		else
			pow[0]=pow[1]=0;
		flag=transaction_generate(device,pow);
#endif
		break;
	case STEP_TANGLE:

		break;
	case STEP_MOVE:

		break;
	}
}































#if 0








uint8 transaction_generate(device_t *device,uint32 *pow)
{
	uint32 i;
	route_t *route;
	transaction_t transaction;

	if (device->transaction_index==TRANSACTION_LENGTH)//transaction full
		return RET_TRANSACTION_FULL;
	if (device->queue_index==QUEUE_LENGTH)//queue full
		return RET_QUEUE_FULL;
	//generate transaction
	transaction.device_index=device->device_index;
	g_index++;
	transaction.index.code=g_index;
	transaction.weight_self=0;
	transaction.weight_accu=0;
	transaction.height=0;
	transaction.depth=0;
	transaction.integral=0;
	transaction.type=TRANSACTION_VALUE;//rand()%2;
	transaction.flag=TRANSACTION_NONE;
	memset(&transaction.trunk,0,sizeof(hash_t));
	memset(&transaction.branch,0,sizeof(hash_t));
	_rand(transaction.plain,KEY_LEN);
	i=_mod(transaction.plain,transaction.plain,&device->pair[4],KEY_LEN,KEY_LEN);
	memset(&transaction.plain[i],0,KEY_LEN-i);
	if (transaction.type==TRANSACTION_TYPE_VALUE)
		transaction_signature(&transaction,device);
	memcpy(transaction.pow,pow,2*sizeof(uint32));
	device->queue[device->queue_index].device_index=device->device_index;
	device->queue[device->queue_index].info=INFO_TRANSACTION;
	memcpy((void *)device->queue[device->queue_index].buffer,&transaction,sizeof(transaction_t));
	//send transaction
	route=device->route;
	while(route)
	{
		if (g_device[route->device_index].queue_index==QUEUE_LENGTH)
		{
			route=route->next;
			continue;
		}
		g_device[route->device_index].queue[g_device[route->device_index].queue_index].device_index=device->device_index;
		g_device[route->device_index].queue[g_device[route->device_index].queue_index].info=INFO_TRANSACTION;
		memcpy((void *)g_device[route->device_index].queue[g_device[route->device_index].queue_index].buffer,(void *)device->queue[device->queue_index].buffer,sizeof(transaction_t));
		g_device[route->device_index].queue_index++;
		route=route->next;
	}
	device->queue_index++;

	return 0;
}

uint8 transaction_recv(device_t *device)
{
	uint32 i;

	if (!device->queue_index)//queue empty
		return RET_QUEUE_EMPTY;
	for (i=0;i<device->queue_index;i++)
		if (device->queue[i].info==INFO_TRANSACTION)
			break;
	if (i==device->queue_index)//no transaction in queue
		return RET_TRANSACTION_NONE;
	if (device->transaction_index==TRANSACTION_LENGTH)//transaction full
		return RET_TRANSACTION_FULL;
	//update transaction
	memcpy(&device->transaction[device->transaction_index],(void *)device->queue[i].buffer,sizeof(transaction_t));
	device->transaction_index++;
	//reset queue
	device->queue_index--;
	for (;i<device->queue_index;i++)
		memcpy(&device->queue[i],&device->queue[i+1],sizeof(queue_t));
	memset(&device->queue[device->queue_index],0,sizeof(queue_t));

	return 0;
}

uint8 tangle_join(device_t *device,uint32 trunk,uint32 branch)
{
	//将transaction队列的一项加入tangle队列
	uint32 i;
	route_t *route;
	
	if (!device->transaction_index)//transaction empty
		return RET_TRANSACTION_EMPTY;
	if (device->tangle_index==TANGLE_LENGTH)//tangle full
		return RET_TANGLE_FULL;
	//join tangle
	memcpy(&device->tangle[device->tangle_index],&device->transaction[0],sizeof(transaction_t));
	device->tangle[device->tangle_index].flag=TRANSACTION_TIP;
	if (device->tangle_index)
	{
		memcpy(&device->tangle[device->tangle_index].trunk,&device->tangle[trunk].index,sizeof(hash_t));
		memcpy(&device->tangle[device->tangle_index].branch,&device->tangle[branch].index,sizeof(hash_t));
		device->tangle[trunk].flag=TRANSACTION_TANGLE;
		device->tangle[branch].flag=TRANSACTION_TANGLE;
	}
	//reset transaction
	device->transaction_index--;
	for (i=0;i<device->transaction_index;i++)
		memcpy(&device->transaction[i],&device->transaction[i+1],sizeof(transaction_t));
	memset(&device->transaction[device->transaction_index],0,sizeof(transaction_t));
	//send tangle
	route=device->route;
	while(route)
	{
		if (g_device[route->device_index].queue_index==QUEUE_LENGTH)
		{
			route=route->next;
			continue;
		}
		g_device[route->device_index].queue[g_device[route->device_index].queue_index].device_index=device->device_index;
		g_device[route->device_index].queue[g_device[route->device_index].queue_index].info=INFO_TANGLE;
		memcpy((void *)g_device[route->device_index].queue[g_device[route->device_index].queue_index].buffer,&device->tangle[device->tangle_index],sizeof(transaction_t));
		g_device[route->device_index].queue_index++;
		route=route->next;
	}
	device->tangle_index++;

	return 0;
}

uint8 tangle_check(void)
{
	//检查各设备的tangle是否一致
	uint32 i,j,k,r;

	for (i=0;i<g_devicenum;i++)
		if (g_device[i].dag_index)
		{
			for (j=i+1;j<g_devicenum;j++)
				if (g_device[i].dag_index==g_device[j].dag_index)
				{
					r=math_min(g_device[i].tangle_index,g_device[j].tangle_index);
					for (k=0;k<r;k++)
						if (g_device[i].tangle[k].device_index!=g_device[j].tangle[k].device_index || memcmp(&g_device[i].tangle[k].index,&g_device[j].tangle[k].index,sizeof(hash_t)) || memcmp(&g_device[i].tangle[k].trunk,&g_device[j].tangle[k].trunk,sizeof(hash_t)) || memcmp(&g_device[i].tangle[k].branch,&g_device[j].tangle[k].branch,sizeof(hash_t)))
							return 1;
				}
		}

	return 0;
}

uint8 tangle_check(device_t *device,transaction_t *transaction)
{
	//检查tangle中是否有存在参照的transaction
	uint32 i;

	if (!device->tangle_index)//tangle empty
		return 0;
	for (i=0;i<device->tangle_index;i++)
		if (!memcmp((void *)&device->tangle[i].index,&transaction->index,sizeof(hash_t)))
			break;
	if (i==device->tangle_index)//no same transaction in tangle
		return 1;

	return 0;
}

uint8 tangle_recv(device_t *device)
{
	uint32 i;
	uint8 flag;

	if (!device->queue_index)//queue empty
		return RET_QUEUE_EMPTY;
	if (device->tangle_index==TANGLE_LENGTH)//tangle full
		return RET_TANGLE_FULL;
	for (i=0;i<device->queue_index;i++)
		if (device->queue[i].info==INFO_TANGLE)
			break;
	if (i==device->queue_index)//no tangle in queue
		return RET_TANGLE_NONE;
	//check tangle's existance
	flag=tangle_check(device,(transaction_t *)device->queue[i].buffer);
	if (flag)//no transaction in tangle
	{
		//update tangle
		memcpy(&device->tangle[device->tangle_index],(void *)device->queue[i].buffer,sizeof(transaction_t));
		device->tangle_index++;
	}
	//reset queue
	device->queue_index--;
	for (;i<device->queue_index;i++)
		memcpy(&device->queue[i],&device->queue[i+1],sizeof(queue_t));
	memset(&device->queue[device->queue_index],0,sizeof(queue_t));

	return 0;
}



#endif