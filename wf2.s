
src/disk.o:     file format elf32-i386


Disassembly of section .text:

000008f0 <ardaos_write_file+0x4c0>:
     8f0:	fa                   	cli
     8f1:	c1 e2 08             	shl    edx,0x8
     8f4:	8d 1c 02             	lea    ebx,[edx+eax*1]
     8f7:	ba ff ff ff ff       	mov    edx,0xffffffff
     8fc:	66 89 54 44 20       	mov    WORD PTR [esp+eax*2+0x20],dx
     901:	83 ec 04             	sub    esp,0x4
     904:	8d 44 24 24          	lea    eax,[esp+0x24]
     908:	50                   	push   eax
     909:	6a 01                	push   0x1
     90b:	51                   	push   ecx
     90c:	e8 fc ff ff ff       	call   90d <ardaos_write_file+0x4dd>
     911:	83 c4 10             	add    esp,0x10
     914:	80 3d 10 00 00 00 01 	cmp    BYTE PTR ds:0x10,0x1
     91b:	76 1d                	jbe    93a <ardaos_write_file+0x50a>
     91d:	83 ec 04             	sub    esp,0x4
     920:	8d 44 24 24          	lea    eax,[esp+0x24]
     924:	50                   	push   eax
     925:	6a 01                	push   0x1
     927:	8b 44 24 20          	mov    eax,DWORD PTR [esp+0x20]
     92b:	03 44 24 18          	add    eax,DWORD PTR [esp+0x18]
     92f:	01 f8                	add    eax,edi
     931:	50                   	push   eax
     932:	e8 fc ff ff ff       	call   933 <ardaos_write_file+0x503>
     937:	83 c4 10             	add    esp,0x10
     93a:	89 ea                	mov    edx,ebp
     93c:	8b bc 24 40 04 00 00 	mov    edi,DWORD PTR [esp+0x440]
     943:	31 c0                	xor    eax,eax
     945:	c1 e2 05             	shl    edx,0x5
     948:	01 f2                	add    edx,esi
     94a:	0f b6 0c 07          	movzx  ecx,BYTE PTR [edi+eax*1]
     94e:	88 0c 02             	mov    BYTE PTR [edx+eax*1],cl
     951:	83 c0 01             	add    eax,0x1
     954:	83 f8 08             	cmp    eax,0x8
     957:	75 f1                	jne    94a <ardaos_write_file+0x51a>
     959:	8b 84 24 44 04 00 00 	mov    eax,DWORD PTR [esp+0x444]
     960:	8b bc 24 44 04 00 00 	mov    edi,DWORD PTR [esp+0x444]
     967:	0f b7 10             	movzx  edx,WORD PTR [eax]
     96a:	89 e8                	mov    eax,ebp
     96c:	c1 e0 05             	shl    eax,0x5
     96f:	66 89 94 04 28 02 00 	mov    WORD PTR [esp+eax*1+0x228],dx
     976:	00 
     977:	0f b6 57 02          	movzx  edx,BYTE PTR [edi+0x2]
     97b:	c6 84 04 2b 02 00 00 	mov    BYTE PTR [esp+eax*1+0x22b],0x0
     982:	00 
     983:	88 94 04 2a 02 00 00 	mov    BYTE PTR [esp+eax*1+0x22a],dl
     98a:	c7 84 04 36 02 00 00 	mov    DWORD PTR [esp+eax*1+0x236],0x0
     991:	00 00 00 00 
     995:	e9 e2 fc ff ff       	jmp    67c <ardaos_write_file+0x24c>
     99a:	8d b6 00 00 00 00    	lea    esi,[esi+0x0]

000009a0 <ardaos_list_files>:
     9a0:	55                   	push   ebp
     9a1:	57                   	push   edi
     9a2:	56                   	push   esi
     9a3:	53                   	push   ebx
     9a4:	81 ec 44 02 00 00    	sub    esp,0x244
     9aa:	0f b7 05 11 00 00 00 	movzx  eax,WORD PTR ds:0x11
     9b1:	8b b4 24 58 02 00 00 	mov    esi,DWORD PTR [esp+0x258]
     9b8:	c1 f8 04             	sar    eax,0x4
     9bb:	89 c1                	mov    ecx,eax
     9bd:	b8 20 00 00 00       	mov    eax,0x20
     9c2:	0f 45 c1             	cmovne eax,ecx
     9c5:	89 44 24 14          	mov    DWORD PTR [esp+0x14],eax
     9c9:	68 00 00 00 00       	push   0x0
     9ce:	56                   	push   esi
     9cf:	e8 fc ff ff ff       	call   9d0 <ardaos_list_files+0x30>
     9d4:	83 c4 10             	add    esp,0x10
     9d7:	31 c9                	xor    ecx,ecx
     9d9:	c7 44 24 08 00 00 00 	mov    DWORD PTR [esp+0x8],0x0
     9e0:	00 
     9e1:	8b 7c 24 08          	mov    edi,DWORD PTR [esp+0x8]
     9e5:	03 3d 00 00 00 00    	add    edi,DWORD PTR ds:0x0
     9eb:	ba f6 01 00 00       	mov    edx,0x1f6
     9f0:	89 f8                	mov    eax,edi
     9f2:	c1 e8 18             	shr    eax,0x18
     9f5:	83 e0 0f             	and    eax,0xf
     9f8:	83 c8 e0             	or     eax,0xffffffe0
     9fb:	ee                   	out    dx,al
     9fc:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]
     a00:	ba f7 01 00 00       	mov    edx,0x1f7
     a05:	ec                   	in     al,dx
     a06:	84 c0                	test   al,al
     a08:	78 f6                	js     a00 <ardaos_list_files+0x60>
     a0a:	b8 01 00 00 00       	mov    eax,0x1
     a0f:	ba f2 01 00 00       	mov    edx,0x1f2
     a14:	ee                   	out    dx,al
     a15:	ba f3 01 00 00       	mov    edx,0x1f3
     a1a:	89 f8                	mov    eax,edi
     a1c:	ee                   	out    dx,al
     a1d:	89 f8                	mov    eax,edi
     a1f:	ba f4 01 00 00       	mov    edx,0x1f4
     a24:	c1 e8 08             	shr    eax,0x8
     a27:	ee                   	out    dx,al
     a28:	89 f8                	mov    eax,edi
     a2a:	ba f5 01 00 00       	mov    edx,0x1f5
     a2f:	c1 e8 10             	shr    eax,0x10
     a32:	ee                   	out    dx,al
     a33:	b8 20 00 00 00       	mov    eax,0x20
     a38:	ba f7 01 00 00       	mov    edx,0x1f7
     a3d:	ee                   	out    dx,al
     a3e:	66 90                	xchg   ax,ax
     a40:	ba f7 01 00 00       	mov    edx,0x1f7
     a45:	ec                   	in     al,dx
     a46:	84 c0                	test   al,al
     a48:	78 f6                	js     a40 <ardaos_list_files+0xa0>
     a4a:	a8 08                	test   al,0x8
     a4c:	74 f2                	je     a40 <ardaos_list_files+0xa0>
     a4e:	8d 6c 24 30          	lea    ebp,[esp+0x30]
     a52:	ba f0 01 00 00       	mov    edx,0x1f0
     a57:	89 ef                	mov    edi,ebp
     a59:	8d b4 26 00 00 00 00 	lea    esi,[esi+eiz*1+0x0]
     a60:	66 ed                	in     ax,dx
     a62:	66 89 07             	mov    WORD PTR [edi],ax
     a65:	83 c7 02             	add    edi,0x2
     a68:	8d 84 24 30 02 00 00 	lea    eax,[esp+0x230]
     a6f:	39 c7                	cmp    edi,eax
     a71:	75 ed                	jne    a60 <ardaos_list_files+0xc0>
     a73:	89 4c 24 04          	mov    DWORD PTR [esp+0x4],ecx
     a77:	8d 7c 24 14          	lea    edi,[esp+0x14]
     a7b:	eb 35                	jmp    ab2 <ardaos_list_files+0x112>
     a7d:	83 ec 08             	sub    esp,0x8
     a80:	68 1f 00 00 00       	push   0x1f
     a85:	56                   	push   esi
     a86:	e8 fc ff ff ff       	call   a87 <ardaos_list_files+0xe7>
     a8b:	83 c4 10             	add    esp,0x10
     a8e:	66 90                	xchg   ax,ax
     a90:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     a97:	00 
     a98:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     a9f:	00 
     aa0:	83 c5 20             	add    ebp,0x20
     aa3:	8d 84 24 30 02 00 00 	lea    eax,[esp+0x230]
     aaa:	39 c5                	cmp    ebp,eax
     aac:	0f 84 d8 00 00 00    	je     b8a <ardaos_list_files+0x1ea>
     ab2:	0f b6 55 00          	movzx  edx,BYTE PTR [ebp+0x0]
     ab6:	84 d2                	test   dl,dl
     ab8:	74 e6                	je     aa0 <ardaos_list_files+0x100>
     aba:	80 fa e5             	cmp    dl,0xe5
     abd:	74 e1                	je     aa0 <ardaos_list_files+0x100>
     abf:	0f b6 55 0b          	movzx  edx,BYTE PTR [ebp+0xb]
     ac3:	80 fa 0f             	cmp    dl,0xf
     ac6:	74 d8                	je     aa0 <ardaos_list_files+0x100>
     ac8:	80 fa 08             	cmp    dl,0x8
     acb:	74 d3                	je     aa0 <ardaos_list_files+0x100>
     acd:	8b 4d 00             	mov    ecx,DWORD PTR [ebp+0x0]
     ad0:	c6 44 24 1c 00       	mov    BYTE PTR [esp+0x1c],0x0
     ad5:	c6 44 24 13 00       	mov    BYTE PTR [esp+0x13],0x0
     ada:	89 4c 24 14          	mov    DWORD PTR [esp+0x14],ecx
     ade:	8b 4d 04             	mov    ecx,DWORD PTR [ebp+0x4]
     ae1:	83 44 24 04 01       	add    DWORD PTR [esp+0x4],0x1
     ae6:	83 ec 08             	sub    esp,0x8
     ae9:	89 4c 24 20          	mov    DWORD PTR [esp+0x20],ecx
     aed:	0f b7 4d 08          	movzx  ecx,WORD PTR [ebp+0x8]
     af1:	66 89 4c 24 18       	mov    WORD PTR [esp+0x18],cx
     af6:	0f b6 4d 0a          	movzx  ecx,BYTE PTR [ebp+0xa]
     afa:	88 4c 24 1a          	mov    BYTE PTR [esp+0x1a],cl
     afe:	68 1c 00 00 00       	push   0x1c
     b03:	56                   	push   esi
     b04:	88 54 24 13          	mov    BYTE PTR [esp+0x13],dl
     b08:	e8 fc ff ff ff       	call   b09 <ardaos_list_files+0x169>
     b0d:	59                   	pop    ecx
     b0e:	5b                   	pop    ebx
     b0f:	57                   	push   edi
     b10:	56                   	push   esi
     b11:	e8 fc ff ff ff       	call   b12 <ardaos_list_files+0x172>
     b16:	83 c4 10             	add    esp,0x10
     b19:	f6 44 24 03 10       	test   BYTE PTR [esp+0x3],0x10
     b1e:	0f 85 59 ff ff ff    	jne    a7d <ardaos_list_files+0xdd>
     b24:	83 ec 08             	sub    esp,0x8
     b27:	83 c5 20             	add    ebp,0x20
     b2a:	68 2c 00 00 00       	push   0x2c
     b2f:	56                   	push   esi
     b30:	e8 fc ff ff ff       	call   b31 <ardaos_list_files+0x191>
     b35:	58                   	pop    eax
     b36:	5a                   	pop    edx
     b37:	8d 44 24 18          	lea    eax,[esp+0x18]
     b3b:	50                   	push   eax
     b3c:	56                   	push   esi
     b3d:	e8 fc ff ff ff       	call   b3e <ardaos_list_files+0x19e>
     b42:	59                   	pop    ecx
     b43:	5b                   	pop    ebx
     b44:	8d 54 24 28          	lea    edx,[esp+0x28]
     b48:	52                   	push   edx
     b49:	ff 75 fc             	push   DWORD PTR [ebp-0x4]
     b4c:	e8 fc ff ff ff       	call   b4d <ardaos_list_files+0x1ad>
     b51:	58                   	pop    eax
     b52:	5a                   	pop    edx
     b53:	68 2e 00 00 00       	push   0x2e
     b58:	56                   	push   esi
     b59:	e8 fc ff ff ff       	call   b5a <ardaos_list_files+0x1ba>
     b5e:	59                   	pop    ecx
     b5f:	5b                   	pop    ebx
     b60:	8d 54 24 28          	lea    edx,[esp+0x28]
     b64:	52                   	push   edx
     b65:	56                   	push   esi
     b66:	e8 fc ff ff ff       	call   b67 <ardaos_list_files+0x1c7>
     b6b:	58                   	pop    eax
     b6c:	5a                   	pop    edx
     b6d:	68 33 00 00 00       	push   0x33
     b72:	56                   	push   esi
     b73:	e8 fc ff ff ff       	call   b74 <ardaos_list_files+0x1d4>
     b78:	83 c4 10             	add    esp,0x10
     b7b:	8d 84 24 30 02 00 00 	lea    eax,[esp+0x230]
     b82:	39 c5                	cmp    ebp,eax
     b84:	0f 85 28 ff ff ff    	jne    ab2 <ardaos_list_files+0x112>
     b8a:	83 44 24 08 01       	add    DWORD PTR [esp+0x8],0x1
     b8f:	8b 4c 24 04          	mov    ecx,DWORD PTR [esp+0x4]
     b93:	8b 44 24 08          	mov    eax,DWORD PTR [esp+0x8]
     b97:	39 44 24 0c          	cmp    DWORD PTR [esp+0xc],eax
     b9b:	0f 85 40 fe ff ff    	jne    9e1 <ardaos_list_files+0x41>
     ba1:	85 c9                	test   ecx,ecx
     ba3:	74 0b                	je     bb0 <ardaos_list_files+0x210>
     ba5:	81 c4 3c 02 00 00    	add    esp,0x23c
     bab:	5b                   	pop    ebx
     bac:	5e                   	pop    esi
     bad:	5f                   	pop    edi
     bae:	5d                   	pop    ebp
     baf:	c3                   	ret
     bb0:	83 ec 08             	sub    esp,0x8
     bb3:	68 3b 00 00 00       	push   0x3b
     bb8:	56                   	push   esi
     bb9:	e8 fc ff ff ff       	call   bba <ardaos_list_files+0x21a>
     bbe:	83 c4 10             	add    esp,0x10
     bc1:	81 c4 3c 02 00 00    	add    esp,0x23c
     bc7:	5b                   	pop    ebx
     bc8:	5e                   	pop    esi
     bc9:	5f                   	pop    edi
     bca:	5d                   	pop    ebp
     bcb:	c3                   	ret
     bcc:	8d 74 26 00          	lea    esi,[esi+eiz*1+0x0]

00000bd0 <ardaos_delete_file>:
     bd0:	55                   	push   ebp
     bd1:	57                   	push   edi
     bd2:	56                   	push   esi
     bd3:	53                   	push   ebx
     bd4:	bb f7 01 00 00       	mov    ebx,0x1f7
     bd9:	81 ec 1c 04 00 00    	sub    esp,0x41c
     bdf:	0f b7 05 11 00 00 00 	movzx  eax,WORD PTR ds:0x11
     be6:	c7 44 24 04 00 00 00 	mov    DWORD PTR [esp+0x4],0x0
     bed:	00 
     bee:	8b bc 24 30 04 00 00 	mov    edi,DWORD PTR [esp+0x430]
     bf5:	c1 f8 04             	sar    eax,0x4
     bf8:	89 c6                	mov    esi,eax
     bfa:	b8 20 00 00 00       	mov    eax,0x20
     bff:	0f 45 c6             	cmovne eax,esi
     c02:	89 44 24 08          	mov    DWORD PTR [esp+0x8],eax
     c06:	8b 4c 24 04          	mov    ecx,DWORD PTR [esp+0x4]
     c0a:	03 0d 00 00 00 00    	add    ecx,DWORD PTR ds:0x0
     c10:	ba f6 01 00 00       	mov    edx,0x1f6
     c15:	89 c8                	mov    eax,ecx
     c17:	c1 e8 18             	shr    eax,0x18
     c1a:	83 e0 0f             	and    eax,0xf
     c1d:	83 c8 e0             	or     eax,0xffffffe0
     c20:	ee                   	out    dx,al
     c21:	8d b4 26 00 00 00 00 	lea    esi,[esi+eiz*1+0x0]
     c28:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     c2f:	00 
     c30:	89 da                	mov    edx,ebx
     c32:	ec                   	in     al,dx
     c33:	84 c0                	test   al,al
     c35:	78 f9                	js     c30 <ardaos_delete_file+0x60>
     c37:	b8 01 00 00 00       	mov    eax,0x1
     c3c:	ba f2 01 00 00       	mov    edx,0x1f2
     c41:	ee                   	out    dx,al
     c42:	ba f3 01 00 00       	mov    edx,0x1f3
     c47:	89 c8                	mov    eax,ecx
     c49:	ee                   	out    dx,al
     c4a:	89 c8                	mov    eax,ecx
     c4c:	ba f4 01 00 00       	mov    edx,0x1f4
     c51:	c1 e8 08             	shr    eax,0x8
     c54:	ee                   	out    dx,al
     c55:	89 c8                	mov    eax,ecx
     c57:	ba f5 01 00 00       	mov    edx,0x1f5
     c5c:	c1 e8 10             	shr    eax,0x10
     c5f:	ee                   	out    dx,al
     c60:	b8 20 00 00 00       	mov    eax,0x20
     c65:	89 da                	mov    edx,ebx
     c67:	ee                   	out    dx,al
     c68:	2e 8d b4 26 00 00 00 	lea    esi,cs:[esi+eiz*1+0x0]
     c6f:	00 
     c70:	89 da                	mov    edx,ebx
     c72:	ec                   	in     al,dx
     c73:	84 c0                	test   al,al
     c75:	78 f9                	js     c70 <ardaos_delete_file+0xa0>
     c77:	a8 08                	test   al,0x8
     c79:	74 f5                	je     c70 <ardaos_delete_file+0xa0>
     c7b:	8d                   	.byte 0x8d
     c7c:	84 24 10             	test   BYTE PTR [eax+edx*1],ah
     c7f:	02                   	.byte 0x2
