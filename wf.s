
src/disk.o:     file format elf32-i386


Disassembly of section .text:

00000430 <ardaos_write_file>:
     430:	55                   	push   ebp
     431:	57                   	push   edi
     432:	56                   	push   esi
     433:	53                   	push   ebx
     434:	81 ec 2c 04 00 00    	sub    esp,0x42c
     43a:	8b bc 24 40 04 00 00 	mov    edi,DWORD PTR [esp+0x440]
     441:	85 ff                	test   edi,edi
     443:	0f 84 a3 03 00 00    	je     7ec <ardaos_write_file+0x3bc>
     449:	8b 84 24 40 04 00 00 	mov    eax,DWORD PTR [esp+0x440]
     450:	f6 00 df             	test   BYTE PTR [eax],0xdf
     453:	0f 84 93 03 00 00    	je     7ec <ardaos_write_file+0x3bc>
     459:	8b b4 24 48 04 00 00 	mov    esi,DWORD PTR [esp+0x448]
     460:	85 f6                	test   esi,esi
     462:	0f 84 86 02 00 00    	je     6ee <ardaos_write_file+0x2be>
     468:	81 bc 24 48 04 00 00 	cmp    DWORD PTR [esp+0x448],0x2801
     46f:	01 28 00 00 
     473:	b8 00 02 00 00       	mov    eax,0x200
     478:	0f 42 84 24 48 04 00 	cmovb  eax,DWORD PTR [esp+0x448]
     47f:	00 
     480:	89 84 24 48 04 00 00 	mov    DWORD PTR [esp+0x448],eax
     487:	0f b7 05 11 00 00 00 	movzx  eax,WORD PTR ds:0x11
     48e:	c7 44 24 08 00 00 00 	mov    DWORD PTR [esp+0x8],0x0
     495:	00 
     496:	bb f7 01 00 00       	mov    ebx,0x1f7
     49b:	c1 f8 04             	sar    eax,0x4
     49e:	89 c6                	mov    esi,eax
     4a0:	b8 20 00 00 00       	mov    eax,0x20
     4a5:	0f 45 c6             	cmovne eax,esi
     4a8:	8d b4 24 20 02 00 00 	lea    esi,[esp+0x220]
     4af:	89 44 24 0c          	mov    DWORD PTR [esp+0xc],eax
     4b3:	8b 4c 24 08          	mov    ecx,DWORD PTR [esp+0x8]
     4b7:	03 0d 00 00 00 00    	add    ecx,DWORD PTR ds:0x0
     4bd:	ba f6 01 00 00       	mov    edx,0x1f6
     4c2:	89 c8                	mov    eax,ecx
     4c4:	c1 e8 18             	shr    eax,0x18
     4c7:	83 e0 0f             	and    eax,0xf
     4ca:	83 c8 e0             	or     eax,0xffffffe0
     4cd:	ee                   	out    dx,al
     4ce:	66 90                	xchg   ax,ax
     4d0:	89 da                	mov    edx,ebx
     4d2:	ec                   	in     al,dx
     4d3:	84 c0                	test   al,al
     4d5:	78 f9                	js     4d0 <ardaos_write_file+0xa0>
     4d7:	b8 01 00 00 00       	mov    eax,0x1
     4dc:	ba f2 01 00 00       	mov    edx,0x1f2
     4e1:	ee                   	out    dx,al
     4e2:	ba f3 01 00 00       	mov    edx,0x1f3
     4e7:	89 c8                	mov    eax,ecx
     4e9:	ee                   	out    dx,al
     4ea:	89 c8                	mov    eax,ecx
     4ec:	ba f4 01 00 00       	mov    edx,0x1f4
     4f1:	c1 e8 08             	shr    eax,0x8
     4f4:	ee                   	out    dx,al
     4f5:	89 c8                	mov    eax,ecx
     4f7:	ba f5 01 00 00       	mov    edx,0x1f5
     4fc:	c1 e8 10             	shr    eax,0x10
     4ff:	ee                   	out    dx,al
     500:	b8 20 00 00 00       	mov    eax,0x20
     505:	89 da                	mov    edx,ebx
     507:	ee                   	out    dx,al
     508:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     50f:	00 
     510:	89 da                	mov    edx,ebx
     512:	ec                   	in     al,dx
     513:	84 c0                	test   al,al
     515:	78 f9                	js     510 <ardaos_write_file+0xe0>
     517:	a8 08                	test   al,0x8
     519:	74 f5                	je     510 <ardaos_write_file+0xe0>
     51b:	89 74 24 10          	mov    DWORD PTR [esp+0x10],esi
     51f:	89 f1                	mov    ecx,esi
     521:	ba f0 01 00 00       	mov    edx,0x1f0
     526:	66 90                	xchg   ax,ax
     528:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     52f:	00 
     530:	66 ed                	in     ax,dx
     532:	66 89 01             	mov    WORD PTR [ecx],ax
     535:	83 c1 02             	add    ecx,0x2
     538:	8d 84 24 20 04 00 00 	lea    eax,[esp+0x420]
     53f:	39 c8                	cmp    eax,ecx
     541:	75 ed                	jne    530 <ardaos_write_file+0x100>
     543:	31 ff                	xor    edi,edi
     545:	89 f5                	mov    ebp,esi
     547:	eb 26                	jmp    56f <ardaos_write_file+0x13f>
     549:	8d b4 26 00 00 00 00 	lea    esi,[esi+eiz*1+0x0]
     550:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     557:	00 
     558:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     55f:	00 
     560:	83 c7 01             	add    edi,0x1
     563:	83 c5 20             	add    ebp,0x20
     566:	83 ff 10             	cmp    edi,0x10
     569:	0f 84 91 01 00 00    	je     700 <ardaos_write_file+0x2d0>
     56f:	0f b6 45 00          	movzx  eax,BYTE PTR [ebp+0x0]
     573:	84 c0                	test   al,al
     575:	74 e9                	je     560 <ardaos_write_file+0x130>
     577:	3c e5                	cmp    al,0xe5
     579:	74 e5                	je     560 <ardaos_write_file+0x130>
     57b:	83 ec 04             	sub    esp,0x4
     57e:	6a 08                	push   0x8
     580:	ff b4 24 48 04 00 00 	push   DWORD PTR [esp+0x448]
     587:	55                   	push   ebp
     588:	e8 fc ff ff ff       	call   589 <ardaos_write_file+0x159>
     58d:	83 c4 10             	add    esp,0x10
     590:	85 c0                	test   eax,eax
     592:	75 cc                	jne    560 <ardaos_write_file+0x130>
     594:	83 ec 04             	sub    esp,0x4
     597:	8d 45 08             	lea    eax,[ebp+0x8]
     59a:	6a 03                	push   0x3
     59c:	ff b4 24 4c 04 00 00 	push   DWORD PTR [esp+0x44c]
     5a3:	50                   	push   eax
     5a4:	e8 fc ff ff ff       	call   5a5 <ardaos_write_file+0x175>
     5a9:	83 c4 10             	add    esp,0x10
     5ac:	85 c0                	test   eax,eax
     5ae:	75 b0                	jne    560 <ardaos_write_file+0x130>
     5b0:	89 fd                	mov    ebp,edi
     5b2:	8b 0d 00 00 00 00    	mov    ecx,DWORD PTR ds:0x0
     5b8:	8b 7c 24 10          	mov    edi,DWORD PTR [esp+0x10]
     5bc:	89 e8                	mov    eax,ebp
     5be:	c1 e0 05             	shl    eax,0x5
     5c1:	89 4c 24 10          	mov    DWORD PTR [esp+0x10],ecx
     5c5:	0f b7 9c 04 3a 02 00 	movzx  ebx,WORD PTR [esp+eax*1+0x23a]
     5cc:	00 
     5cd:	b8 ff ff ff ff       	mov    eax,0xffffffff
     5d2:	83 fb 01             	cmp    ebx,0x1
     5d5:	0f 4e d8             	cmovle ebx,eax
     5d8:	8b 44 24 08          	mov    eax,DWORD PTR [esp+0x8]
     5dc:	01 c8                	add    eax,ecx
     5de:	89 c2                	mov    edx,eax
     5e0:	89 44 24 18          	mov    DWORD PTR [esp+0x18],eax
     5e4:	c1 e8 18             	shr    eax,0x18
     5e7:	83 e0 0f             	and    eax,0xf
     5ea:	88 54 24 14          	mov    BYTE PTR [esp+0x14],dl
     5ee:	0f b6 ce             	movzx  ecx,dh
     5f1:	83 c8 e0             	or     eax,0xffffffe0
     5f4:	89 4c 24 0c          	mov    DWORD PTR [esp+0xc],ecx
     5f8:	88 44 24 08          	mov    BYTE PTR [esp+0x8],al
     5fc:	89 d0                	mov    eax,edx
     5fe:	c1 e8 10             	shr    eax,0x10
     601:	89 c1                	mov    ecx,eax
     603:	ba f6 01 00 00       	mov    edx,0x1f6
     608:	0f b6 44 24 08       	movzx  eax,BYTE PTR [esp+0x8]
     60d:	ee                   	out    dx,al
     60e:	ba f7 01 00 00       	mov    edx,0x1f7
     613:	ec                   	in     al,dx
     614:	84 c0                	test   al,al
     616:	78 f6                	js     60e <ardaos_write_file+0x1de>
     618:	b8 01 00 00 00       	mov    eax,0x1
     61d:	ba f2 01 00 00       	mov    edx,0x1f2
     622:	ee                   	out    dx,al
     623:	ba f3 01 00 00       	mov    edx,0x1f3
     628:	0f b6 44 24 14       	movzx  eax,BYTE PTR [esp+0x14]
     62d:	ee                   	out    dx,al
     62e:	ba f4 01 00 00       	mov    edx,0x1f4
     633:	0f b6 44 24 0c       	movzx  eax,BYTE PTR [esp+0xc]
     638:	ee                   	out    dx,al
     639:	ba f5 01 00 00       	mov    edx,0x1f5
     63e:	89 c8                	mov    eax,ecx
     640:	ee                   	out    dx,al
     641:	b8 20 00 00 00       	mov    eax,0x20
     646:	ba f7 01 00 00       	mov    edx,0x1f7
     64b:	ee                   	out    dx,al
     64c:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]
     650:	ec                   	in     al,dx
     651:	84 c0                	test   al,al
     653:	78 fb                	js     650 <ardaos_write_file+0x220>
     655:	a8 08                	test   al,0x8
     657:	74 f7                	je     650 <ardaos_write_file+0x220>
     659:	ba f0 01 00 00       	mov    edx,0x1f0
     65e:	66 90                	xchg   ax,ax
     660:	66 ed                	in     ax,dx
     662:	66 89 07             	mov    WORD PTR [edi],ax
     665:	83 c7 02             	add    edi,0x2
     668:	8d 84 24 20 04 00 00 	lea    eax,[esp+0x420]
     66f:	39 f8                	cmp    eax,edi
     671:	75 ed                	jne    660 <ardaos_write_file+0x230>
     673:	83 fb ff             	cmp    ebx,0xffffffff
     676:	0f 84 96 01 00 00    	je     812 <ardaos_write_file+0x3e2>
     67c:	0f b6 05 0d 00 00 00 	movzx  eax,BYTE PTR ds:0xd
     683:	8d 53 fe             	lea    edx,[ebx-0x2]
     686:	0f af d0             	imul   edx,eax
     689:	03 15 00 00 00 00    	add    edx,DWORD PTR ds:0x0
     68f:	83 fb 01             	cmp    ebx,0x1
     692:	0f 8e 54 01 00 00    	jle    7ec <ardaos_write_file+0x3bc>
     698:	39 54 24 10          	cmp    DWORD PTR [esp+0x10],edx
     69c:	0f 83 4a 01 00 00    	jae    7ec <ardaos_write_file+0x3bc>
     6a2:	83 ec 04             	sub    esp,0x4
     6a5:	ff b4 24 50 04 00 00 	push   DWORD PTR [esp+0x450]
     6ac:	8b 84 24 50 04 00 00 	mov    eax,DWORD PTR [esp+0x450]
     6b3:	05 ff 01 00 00       	add    eax,0x1ff
     6b8:	c1 e8 09             	shr    eax,0x9
     6bb:	50                   	push   eax
     6bc:	52                   	push   edx
     6bd:	e8 fc ff ff ff       	call   6be <ardaos_write_file+0x28e>
     6c2:	89 e9                	mov    ecx,ebp
     6c4:	c1 e1 05             	shl    ecx,0x5
     6c7:	66 89 9c 0c 4a 02 00 	mov    WORD PTR [esp+ecx*1+0x24a],bx
     6ce:	00 
     6cf:	83 c4 0c             	add    esp,0xc
     6d2:	56                   	push   esi
     6d3:	6a 01                	push   0x1
     6d5:	ff 74 24 24          	push   DWORD PTR [esp+0x24]
     6d9:	e8 fc ff ff ff       	call   6da <ardaos_write_file+0x2aa>
     6de:	83 c4 10             	add    esp,0x10
     6e1:	31 c0                	xor    eax,eax
     6e3:	81 c4 2c 04 00 00    	add    esp,0x42c
     6e9:	5b                   	pop    ebx
     6ea:	5e                   	pop    esi
     6eb:	5f                   	pop    edi
     6ec:	5d                   	pop    ebp
     6ed:	c3                   	ret
     6ee:	c7 84 24 48 04 00 00 	mov    DWORD PTR [esp+0x448],0x1
     6f5:	01 00 00 00 
     6f9:	e9 89 fd ff ff       	jmp    487 <ardaos_write_file+0x57>
     6fe:	66 90                	xchg   ax,ax
     700:	83 44 24 08 01       	add    DWORD PTR [esp+0x8],0x1
     705:	8b 44 24 08          	mov    eax,DWORD PTR [esp+0x8]
     709:	39 44 24 0c          	cmp    DWORD PTR [esp+0xc],eax
     70d:	0f 85 a0 fd ff ff    	jne    4b3 <ardaos_write_file+0x83>
     713:	a1 00 00 00 00       	mov    eax,ds:0x0
     718:	8b 5c 24 0c          	mov    ebx,DWORD PTR [esp+0xc]
     71c:	8b 7c 24 10          	mov    edi,DWORD PTR [esp+0x10]
     720:	89 44 24 10          	mov    DWORD PTR [esp+0x10],eax
     724:	01 c3                	add    ebx,eax
     726:	89 5c 24 18          	mov    DWORD PTR [esp+0x18],ebx
     72a:	bb f7 01 00 00       	mov    ebx,0x1f7
     72f:	89 7c 24 1c          	mov    DWORD PTR [esp+0x1c],edi
     733:	89 c7                	mov    edi,eax
     735:	89 f8                	mov    eax,edi
     737:	ba f6 01 00 00       	mov    edx,0x1f6
     73c:	c1 e8 18             	shr    eax,0x18
     73f:	83 e0 0f             	and    eax,0xf
     742:	83 c8 e0             	or     eax,0xffffffe0
     745:	88 44 24 08          	mov    BYTE PTR [esp+0x8],al
     749:	ee                   	out    dx,al
     74a:	89 da                	mov    edx,ebx
     74c:	ec                   	in     al,dx
     74d:	84 c0                	test   al,al
     74f:	78 f9                	js     74a <ardaos_write_file+0x31a>
     751:	b8 01 00 00 00       	mov    eax,0x1
     756:	ba f2 01 00 00       	mov    edx,0x1f2
     75b:	ee                   	out    dx,al
     75c:	89 f8                	mov    eax,edi
     75e:	ba f3 01 00 00       	mov    edx,0x1f3
     763:	88 44 24 14          	mov    BYTE PTR [esp+0x14],al
     767:	ee                   	out    dx,al
     768:	89 f8                	mov    eax,edi
     76a:	ba f4 01 00 00       	mov    edx,0x1f4
     76f:	0f b6 c4             	movzx  eax,ah
     772:	88 44 24 0c          	mov    BYTE PTR [esp+0xc],al
     776:	ee                   	out    dx,al
     777:	89 f9                	mov    ecx,edi
     779:	ba f5 01 00 00       	mov    edx,0x1f5
     77e:	c1 e9 10             	shr    ecx,0x10
     781:	89 c8                	mov    eax,ecx
     783:	ee                   	out    dx,al
     784:	b8 20 00 00 00       	mov    eax,0x20
     789:	89 da                	mov    edx,ebx
     78b:	ee                   	out    dx,al
     78c:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]
     790:	89 da                	mov    edx,ebx
     792:	ec                   	in     al,dx
     793:	84 c0                	test   al,al
     795:	78 f9                	js     790 <ardaos_write_file+0x360>
     797:	a8 08                	test   al,0x8
     799:	74 f5                	je     790 <ardaos_write_file+0x360>
     79b:	89 f5                	mov    ebp,esi
     79d:	ba f0 01 00 00       	mov    edx,0x1f0
     7a2:	8d b6 00 00 00 00    	lea    esi,[esi+0x0]
     7a8:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     7af:	00 
     7b0:	66 ed                	in     ax,dx
     7b2:	66 89 45 00          	mov    WORD PTR [ebp+0x0],ax
     7b6:	83 c5 02             	add    ebp,0x2
     7b9:	8d 84 24 20 04 00 00 	lea    eax,[esp+0x420]
     7c0:	39 e8                	cmp    eax,ebp
     7c2:	75 ec                	jne    7b0 <ardaos_write_file+0x380>
     7c4:	31 ed                	xor    ebp,ebp
     7c6:	89 e8                	mov    eax,ebp
     7c8:	c1 e0 05             	shl    eax,0x5
     7cb:	0f b6 04 06          	movzx  eax,BYTE PTR [esi+eax*1]
     7cf:	84 c0                	test   al,al
     7d1:	74 2d                	je     800 <ardaos_write_file+0x3d0>
     7d3:	3c e5                	cmp    al,0xe5
     7d5:	74 29                	je     800 <ardaos_write_file+0x3d0>
     7d7:	83 c5 01             	add    ebp,0x1
     7da:	83 fd 10             	cmp    ebp,0x10
     7dd:	75 e7                	jne    7c6 <ardaos_write_file+0x396>
     7df:	83 c7 01             	add    edi,0x1
     7e2:	39 7c 24 18          	cmp    DWORD PTR [esp+0x18],edi
     7e6:	0f 85 49 ff ff ff    	jne    735 <ardaos_write_file+0x305>
     7ec:	b8 ff ff ff ff       	mov    eax,0xffffffff
     7f1:	81 c4 2c 04 00 00    	add    esp,0x42c
     7f7:	5b                   	pop    ebx
     7f8:	5e                   	pop    esi
     7f9:	5f                   	pop    edi
     7fa:	5d                   	pop    ebp
     7fb:	c3                   	ret
     7fc:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]
     800:	89 7c 24 18          	mov    DWORD PTR [esp+0x18],edi
     804:	bb ff ff ff ff       	mov    ebx,0xffffffff
     809:	8b 7c 24 1c          	mov    edi,DWORD PTR [esp+0x1c]
     80d:	e9 f1 fd ff ff       	jmp    603 <ardaos_write_file+0x1d3>
     812:	0f b7 05 16 00 00 00 	movzx  eax,WORD PTR ds:0x16
     819:	89 44 24 0c          	mov    DWORD PTR [esp+0xc],eax
     81d:	85 c0                	test   eax,eax
     81f:	74 cb                	je     7ec <ardaos_write_file+0x3bc>
     821:	0f b7 05 0e 00 00 00 	movzx  eax,WORD PTR ds:0xe
     828:	c7 44 24 08 00 00 00 	mov    DWORD PTR [esp+0x8],0x0
     82f:	00 
     830:	bf f7 01 00 00       	mov    edi,0x1f7
     835:	89 44 24 14          	mov    DWORD PTR [esp+0x14],eax
     839:	89 c1                	mov    ecx,eax
     83b:	ba f6 01 00 00       	mov    edx,0x1f6
     840:	b8 e0 ff ff ff       	mov    eax,0xffffffe0
     845:	ee                   	out    dx,al
     846:	89 fa                	mov    edx,edi
     848:	ec                   	in     al,dx
     849:	84 c0                	test   al,al
     84b:	78 f9                	js     846 <ardaos_write_file+0x416>
     84d:	b8 01 00 00 00       	mov    eax,0x1
     852:	ba f2 01 00 00       	mov    edx,0x1f2
     857:	ee                   	out    dx,al
     858:	ba f3 01 00 00       	mov    edx,0x1f3
     85d:	89 c8                	mov    eax,ecx
     85f:	ee                   	out    dx,al
     860:	89 c8                	mov    eax,ecx
     862:	ba f4 01 00 00       	mov    edx,0x1f4
     867:	c1 e8 08             	shr    eax,0x8
     86a:	ee                   	out    dx,al
     86b:	89 c8                	mov    eax,ecx
     86d:	ba f5 01 00 00       	mov    edx,0x1f5
     872:	c1 e8 10             	shr    eax,0x10
     875:	ee                   	out    dx,al
     876:	b8 20 00 00 00       	mov    eax,0x20
     87b:	89 fa                	mov    edx,edi
     87d:	ee                   	out    dx,al
     87e:	66 90                	xchg   ax,ax
     880:	89 fa                	mov    edx,edi
     882:	ec                   	in     al,dx
     883:	84 c0                	test   al,al
     885:	78 f9                	js     880 <ardaos_write_file+0x450>
     887:	a8 08                	test   al,0x8
     889:	74 f5                	je     880 <ardaos_write_file+0x450>
     88b:	8d 5c 24 20          	lea    ebx,[esp+0x20]
     88f:	ba f0 01 00 00       	mov    edx,0x1f0
     894:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]
     898:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     89f:	00 
     8a0:	66 ed                	in     ax,dx
     8a2:	66 89 03             	mov    WORD PTR [ebx],ax
     8a5:	83 c3 02             	add    ebx,0x2
     8a8:	39 f3                	cmp    ebx,esi
     8aa:	75 f4                	jne    8a0 <ardaos_write_file+0x470>
     8ac:	8b 5c 24 08          	mov    ebx,DWORD PTR [esp+0x8]
     8b0:	31 c0                	xor    eax,eax
     8b2:	85 db                	test   ebx,ebx
     8b4:	0f 94 c0             	sete   al
     8b7:	01 c0                	add    eax,eax
     8b9:	66 83 7c 44 20 00    	cmp    WORD PTR [esp+eax*2+0x20],0x0
     8bf:	74 2a                	je     8eb <ardaos_write_file+0x4bb>
     8c1:	83 c0 01             	add    eax,0x1
     8c4:	3d 00 01 00 00       	cmp    eax,0x100
     8c9:	75 ee                	jne    8b9 <ardaos_write_file+0x489>
     8cb:	83 44 24 08 01       	add    DWORD PTR [esp+0x8],0x1
     8d0:	8b 5c 24 0c          	mov    ebx,DWORD PTR [esp+0xc]
     8d4:	83 c1 01             	add    ecx,0x1
     8d7:	39 5c 24 08          	cmp    DWORD PTR [esp+0x8],ebx
     8db:	0f 85 5a ff ff ff    	jne    83b <ardaos_write_file+0x40b>
     8e1:	b8 ff ff ff ff       	mov    eax,0xffffffff
     8e6:	e9 06 ff ff ff       	jmp    7f1 <ardaos_write_file+0x3c1>
     8eb:	8b 7c 24 08          	mov    edi,DWORD PTR [esp+0x8]
     8ef:	89 fa                	mov    edx,edi
     8f1:	c1 e2 08             	shl    edx,0x8
     8f4:	8d 1c 02             	lea    ebx,[edx+eax*1]
     8f7:	ba ff ff ff ff       	mov    edx,0xffffffff
     8fc:	66                   	data16
     8fd:	89                   	.byte 0x89
     8fe:	54                   	push   esp
     8ff:	44                   	inc    esp
