#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string>
#include "proto.h"
#include <gd.h>
extern "C"{
#include "cgi_proto.h"
#include "libtaomee/crypt/qdes.h"
#include "libtaomee/dataformatter/bin_str.h"
#include "libtaomee/conf_parser/config.h"
#include "json/json.h"
}
#include "log.h"
#include "cgic.h"
#include "tcpip.h"
int parse_uploadfile_type(uint32_t* pic_type ,uint32_t* branchtype,uint8_t filedate[9])
{

	uint8_t png_type[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
	uint8_t gif_type_1[6] = {0x47,0x49,0x46,0x38,0x39,0x61};
	uint8_t gif_type_2[6] = {0x47,0x49,0x46,0x38,0x37,0x61};
	switch (filedate[0]){
		case 0xff:
			if (filedate[1]==0xd8){
				*pic_type = 1;
				*branchtype = jpg;	//jpg
				return SUCC;
			}
			break;
		case 0x89:
			if(0 == memcmp(filedate,png_type,8)){
				*pic_type = 1;
				*branchtype = png;	//png  
				return SUCC;
			}
		break;
		case 0x47:
			if(0 == memcmp(filedate,gif_type_1,6) || 0 == memcmp(filedate,gif_type_2,6)){
				*pic_type = 1;
				*branchtype = gif;	//gif
				return SUCC;
			}
		break;
		default:
			*pic_type = 2;
			*branchtype = 0;
		break;
		}
	*pic_type = 2;
	*branchtype = 0;
	return SUCC;
}

gdImagePtr cgi_create_image(int pic_type, int size, void* data)
{
	switch (pic_type) {
	case jpg:
		return gdImageCreateFromJpegPtr(size, data);
	case png:
		return gdImageCreateFromPngPtr(size, data);
	case gif:
		return gdImageCreateFromGifPtr(size, data);
	default:
		CGI_ERROR_LOG("bad pic type\t[%u]", pic_type);
		return NULL;
	}
}


logo_recv_fileServ_t* received_from_fileserver(char* pkg,int pkglen)
{
	int len = *(uint32_t*)pkg;
	if(len != pkglen){
		CGI_ERROR_LOG("cgi receive from fileserver error len[%u,%u]",len,pkglen);
		return NULL;							
	}
	if(pkglen <= 18){
		protocol_t* pkg_recv = (protocol_t*)pkg;
		CGI_ERROR_LOG("upload error.ret:[%u]",pkg_recv->ret);
		return NULL;
	}
	logo_recv_fileServ_t* pkg_recv = (logo_recv_fileServ_t*)(pkg + PROTO_H_SIZE);
	CGI_DEBUG_LOG("UPLOAD_LOGO RECV HOSTID[%u] old_lloccode[%s] thumb_cnt[%u]",
					pkg_recv->hostid,
					pkg_recv->old_lloccode,
					pkg_recv->Thumbcnt);
	int j =0;
	for(uint32_t i =0;i< pkg_recv->Thumbcnt && i <CNT_MAX;i++){	
		char lloccode[LLOCCODE_LEN+1];
		UNPKG_STR(pkg_recv->new_lloccodes,lloccode,j,LLOCCODE_LEN);
		CGI_DEBUG_LOG("UPLOAD_LOGO RECV NEW lloccode[%s]",lloccode);
	}
	return pkg_recv;
}

logo_recv_fileServ_t* connect_with_fileserver(const char* buffer,Ctcp* conn_fileserver,logo_recv_webclient_t *web_pkg, int filelen,uint32_t pic_type,uint32_t branchtype,const char* path)
{
	char tmp[3*1024*1024]; 
	if(web_pkg == NULL){
		CGI_ERROR_LOG("web_pkg == NULL");
		return NULL;
	}	
	memset(tmp, 0, sizeof tmp);  	
	int j = PROTO_H_SIZE;
	PKG_H_UINT32(tmp,web_pkg->channel,j);			//channel
	PKG_H_UINT32(tmp, pic_type, j);            		//type	
	PKG_H_UINT32(tmp, branchtype, j);				//branchtype
	PKG_H_UINT32(tmp,web_pkg->width_limit,j);		//width_limit
	PKG_H_UINT32(tmp,web_pkg->height_limit,j);		//height_limit
	PKG_H_UINT32(tmp,web_pkg->cnt,j);      			//thumb_cnt
	for(uint32_t i =0;i< web_pkg->cnt && i < CNT_MAX;i++){
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].W,j);
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].H,j);
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].start_x,j);
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].start_y,j);
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].thumb_x,j);
		PKG_H_UINT32(tmp,web_pkg->thumb_arg[i].thumb_y,j);
		CGI_DEBUG_LOG("UPLOAD_LOGO SEND[%u %u %u %u %u %u]",web_pkg->thumb_arg[i].W,
															web_pkg->thumb_arg[i].H,
															web_pkg->thumb_arg[i].start_x,
															web_pkg->thumb_arg[i].start_y,
															web_pkg->thumb_arg[i].thumb_x,
															web_pkg->thumb_arg[i].thumb_y);

	}
	PKG_STR(tmp,path,j,MAX_PATH_LEN);			//path
	CGI_DEBUG_LOG("UPLOAD_LOGO SEND [ %u %u %u %u %u %u ]",
							web_pkg->channel,
							pic_type,
							branchtype,
							web_pkg->width_limit,
							web_pkg->height_limit,
							web_pkg->cnt);
	init_proto_head(tmp, web_pkg->userid, proto_fs_cgi_chg_thumb, j);
	char *recv= NULL;
	int len = 0;
	int ret = conn_fileserver->do_net_io((const char *)tmp,j,&recv,&len);
    if(ret != SUCC){
		CGI_ERROR_LOG("cgi connect fileserver error :errorid:%d,ip[%s],port[%d]",ret,conn_fileserver->get_ip(),conn_fileserver->get_port());
		return NULL;		            
	}
	return (received_from_fileserver(recv,len));
}

int parse_web_data(const char* s_data,logo_recv_webclient_t *d_data)
{
	char upload_s_tmp[256]={0};
	char d_tmp[256]={0};
	uint32_t j =0;
	int s_len = strlen(s_data);
	str2hex(s_data,s_len,upload_s_tmp);
	upload_s_tmp[s_len/2] = '\0';
	des_decrypt_n(KEY,upload_s_tmp,d_tmp,s_len/16);		
	UNPKG_H_UINT32(d_tmp,d_data->userid,j);
	UNPKG_H_UINT32(d_tmp,d_data->channel,j);
	UNPKG_H_UINT32(d_tmp,d_data->ip,j);
	UNPKG_H_UINT32(d_tmp,d_data->time,j);
	UNPKG_H_UINT32(d_tmp,d_data->width_limit,j);
	UNPKG_H_UINT32(d_tmp,d_data->height_limit,j);
	UNPKG_H_UINT32(d_tmp,d_data->cnt,j);
	CGI_DEBUG_LOG("UPLOAD_LOGO RECV WEB[%u %u %u %u %u %u %u]",d_data->userid,d_data->channel,d_data->ip,d_data->time,d_data->width_limit,d_data->height_limit,d_data->cnt);
	for(uint32_t i=0;i < d_data->cnt && i < CNT_MAX;i++){
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].W,j);
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].H,j);
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].start_x,j);
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].start_y,j);
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].thumb_x,j);
		UNPKG_H_UINT32(d_tmp,d_data->thumb_arg[i].thumb_y,j);
		CGI_DEBUG_LOG("UPLOAD_LOGO RECV WEB[%u %u %u %u %u %u]",d_data->thumb_arg[i].W,
																d_data->thumb_arg[i].H,
																d_data->thumb_arg[i].start_x,
																d_data->thumb_arg[i].start_y,
																d_data->thumb_arg[i].thumb_x,
																d_data->thumb_arg[i].thumb_y);
	}
	return SUCC;
}
void cgi_print_result(logo_recv_fileServ_t* recv_data,const char* filename,uint32_t pic_type,uint32_t branchtype,int result,uint32_t reason_id)
{	
	struct json_object *my_object;
	struct json_object *sub_array;
	my_object = json_object_new_object();
	sub_array = json_object_new_array();
	char str_tmp[512] = {0};
	uint32_t i =0,j=0;
	if(result == FAIL){
		json_object_object_add(my_object,"result",json_object_new_string("fail"));
		json_object_object_add(my_object,"filename",json_object_new_string(filename));
		json_object_object_add(my_object,"reason",json_object_new_int(reason_id));
		goto PRINT_END;
	}
	if(recv_data == NULL){
		json_object_object_add(my_object,"result",json_object_new_string("fail"));
		json_object_object_add(my_object,"filename",json_object_new_string(filename));
		json_object_object_add(my_object,"reason",json_object_new_int(reason_id));
		goto PRINT_END;
	}
	json_object_object_add(my_object,"result",json_object_new_string("success"));
	json_object_object_add(my_object,"filename",json_object_new_string(filename));
	json_object_object_add(my_object,"hostid",json_object_new_int(recv_data->hostid));
	json_object_object_add(my_object,"lloccode",json_object_new_string(recv_data->old_lloccode));
	json_object_object_add(my_object,"Thumbcnt",json_object_new_int(recv_data->Thumbcnt));
	if(recv_data->Thumbcnt == 0){
		goto PRINT_END;
	}
	for(i = 0;i < recv_data->Thumbcnt && i < CNT_MAX; i++){
		char lloccode[LLOCCODE_LEN+1] = {0};
		UNPKG_STR(recv_data->new_lloccodes,lloccode,j,LLOCCODE_LEN);
		sprintf(str_tmp,"%s",lloccode);
		json_object_array_add(sub_array, json_object_new_string(str_tmp));

	}
	json_object_object_add(my_object,"new_lloccode",sub_array);
PRINT_END:
	fprintf(cgiOut,"%s\n",json_object_to_json_string(my_object));
	json_object_put(sub_array);
	json_object_put(my_object);
	return ;
}
int cgiMain(void){
    cgiFilePtr file;
    char fileNameOnServer[512]={0};
    char buffer[pic_max_len];
    int got = 0;
	int filelen = 0;
	Ctcp *conn_fileserver = NULL; 	
	char s_data[512]; 
	logo_recv_webclient_t data;
	uint32_t pic_type = 0;
	uint32_t branchtype = 0;
	logo_recv_fileServ_t* recv_data = NULL;
	log_init("/cgi/log/",(log_lvl_t)8,log_size,0,"");
	cgiHeaderContentType((char *)"text/html");
	  
	cgiFormString((char*)"session_key",s_data,512);
	if(strlen(s_data) == 0){
		CGI_ERROR_LOG("session_key error:len[%u] [%s]",strlen(s_data),s_data);
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,session_key_err);
		return FAIL;
	}
	parse_web_data(s_data,&data);
	
	if(data.cnt >CNT_MAX){
		CGI_ERROR_LOG("thumb cnt error[%u]",data.cnt);
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,thumb_cnt_err);
		return FAIL;
	}
	if(cgiFormFileName((char *)"file",fileNameOnServer, sizeof(fileNameOnServer)) !=cgiFormSuccess){   
		CGI_ERROR_LOG("could not retrieve filename, file: %s", fileNameOnServer);
		cgi_print_result(recv_data,"",pic_type,branchtype,FAIL,retrieve_filename_err);
		return FAIL;
    }
	char tfilename[128];
	if(cgiFormSuccess != cgiGetFilePath((char *)"file",tfilename,sizeof(tfilename))){
		CGI_ERROR_LOG("GET FILE PATH ERROR");
		cgi_print_result(recv_data,"",pic_type,branchtype,FAIL,retrieve_filename_err);
		return FAIL;
	}
	
	cgiFormFileSize((char *)"file", &filelen);
	if(filelen > pic_max_len){
		CGI_ERROR_LOG("file size is too big");
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,image_size_err);
		return FAIL;
	}

    if (cgiFormFileOpen((char *)"file", &file) != cgiFormSuccess) {
		CGI_ERROR_LOG("could not open the file");
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,open_file_err);
        return FAIL;
    }
	if(-1 == config_init("/opt/taomee/cgi_conf/bench.conf")){
		CGI_ERROR_LOG("read conf_file error");
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,unknown_err);
		return FAIL;
	}
	char *bind_ip = config_get_strval("bind_ip");
	if(data.ip != inet_addr(bind_ip)){
		struct in_addr addr1;
		memcpy(&addr1, &(data.ip), 4);
		CGI_ERROR_LOG("request is forbidden bind_ip[%d %s],req_ip[%u %s]",inet_addr(bind_ip),bind_ip,data.ip,inet_ntoa(addr1));
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,hostid_err);
		return FAIL;
	}
	time_t now_time; 
	now_time = time(NULL);  
	if((now_time - data.time) > 5*60){
		CGI_ERROR_LOG("upload timeout nowtime[%u] data.time[%u]",now_time,data.time);
		cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,timeout_err);
		return FAIL;
	}
	conn_fileserver = new Ctcp(config_get_strval("bind_fileserver_ip"),config_get_intval("bind_fileserver_port", 0),0,10);	
	if(cgiFormFileRead(file, buffer, pic_max_len, &got) ==cgiFormSuccess){
		if(got > 0){
			uint8_t filedate[8];
			memcpy(filedate,(uint8_t*)buffer,8);
			parse_uploadfile_type(&pic_type ,&branchtype,filedate);
			if(pic_type == 2){
				CGI_ERROR_LOG("image type error.");
				delete conn_fileserver;
				cgiFormFileClose(file);
				cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,image_type_err);
				config_exit();
				return FAIL;
			}
			if(NULL == cgi_create_image(branchtype, got, buffer)){
				CGI_ERROR_LOG("invalid file");
				delete conn_fileserver;
				cgiFormFileClose(file);
				cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,invalid_file);
				config_exit();
				return FAIL;
			}
			cgiFormFileClose(file);
			recv_data = connect_with_fileserver(buffer,conn_fileserver,&data,filelen,pic_type,branchtype,tfilename);
      		if(recv_data == NULL){
				CGI_ERROR_LOG("cgi connect fileserver error");
				delete conn_fileserver;
				cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,net_err);
				config_exit();
				return FAIL;
			}
			delete conn_fileserver;
			cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,SUCC,0);
			config_exit();
			return SUCC;
		}

	}
	CGI_ERROR_LOG("cgi read file error:got=%d",got);
	delete conn_fileserver;
	cgiFormFileClose(file);
	cgi_print_result(recv_data,fileNameOnServer,pic_type,branchtype,FAIL,read_file_err);
	config_exit();
	return FAIL;	
}
    
