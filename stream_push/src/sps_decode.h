/// fanxiushu 2018-06-24, parse h264 sps pps, copy from internet source,这个最终是从ffmpeg修改得来的
#pragma once

typedef struct SPS {
	int profile_idc;
	int constraint_set0_flag;
	int constraint_set1_flag;
	int constraint_set2_flag;
	int constraint_set3_flag;
	int reserved_zero_4bits;
	int level_idc;
	int seq_parameter_set_id;						//ue(v)
	int	chroma_format_idc;							//ue(v)
	int	separate_colour_plane_flag;					//u(1)
	int	bit_depth_luma_minus8;						//0 ue(v) 
	int	bit_depth_chroma_minus8;					//0 ue(v) 
	int	qpprime_y_zero_transform_bypass_flag;		//0 u(1) 
	int seq_scaling_matrix_present_flag;			//0 u(1)
	int	seq_scaling_list_present_flag[12];
	int	UseDefaultScalingMatrix4x4Flag[6];
	int	UseDefaultScalingMatrix8x8Flag[6];
	int	ScalingList4x4[6][16];
	int	ScalingList8x8[6][64];
	int log2_max_frame_num_minus4;					//0	ue(v)
	int pic_order_cnt_type;						//0 ue(v)
	int log2_max_pic_order_cnt_lsb_minus4;				//
	int	delta_pic_order_always_zero_flag;           //u(1)
	int	offset_for_non_ref_pic;                     //se(v)
	int	offset_for_top_to_bottom_field;            //se(v)
	int	num_ref_frames_in_pic_order_cnt_cycle;    //ue(v)	
	int	offset_for_ref_frame_array[16];           //se(v)
	int num_ref_frames;                           //ue(v)
	int	gaps_in_frame_num_value_allowed_flag;    //u(1)
	int	pic_width_in_mbs_minus1;                //ue(v)
	int	pic_height_in_map_units_minus1;         //u(1)
	int	frame_mbs_only_flag;  	                //0 u(1) 
	int	mb_adaptive_frame_field_flag;           //0 u(1) 
	int	direct_8x8_inference_flag;              //0 u(1) 
	int	frame_cropping_flag;                    //u(1)
	int	frame_crop_left_offset;                //ue(v)
	int	frame_crop_right_offset;                //ue(v)
	int	frame_crop_top_offset;                  //ue(v)
	int	frame_crop_bottom_offset;	            //ue(v)
	int vui_parameters_present_flag;            //u(1)
//	vui_parameters_t vui_parameters;

	////
	int pic_width;
	int pic_height; ///

}SPS;

/***
* Picture parameter set
*/
typedef struct PPS {
	int pic_parameter_set_id;
	int seq_parameter_set_id;
	int entropy_coding_mode_flag;
	int pic_order_present_flag;
	int num_slice_groups_minus1;
	int slice_group_map_type;
	int run_length_minus1[32];
	int top_left[32];
	int bottom_right[32];
	int slice_group_change_direction_flag;
	int slice_group_change_rate_minus1;
	int pic_size_in_map_units_minus1;
	int slice_group_id[32];
	int num_ref_idx_10_active_minus1;
	int num_ref_idx_11_active_minus1;
	int weighted_pred_flag;
	int weighted_bipred_idc;
	int pic_init_qp_minus26;
	int pic_init_qs_minus26;
	int chroma_qp_index_offset;
	int deblocking_filter_control_present_flag;
	int constrained_intra_pred_flag;
	int redundant_pic_cnt_present_flag;
	int transform_8x8_mode_flag;
	int pic_scaling_matrix_present_flag;
	int pic_scaling_list_present_flag[32];
	int second_chroma_qp_index_offset;
	int	UseDefaultScalingMatrix4x4Flag[6];
	int	UseDefaultScalingMatrix8x8Flag[6];
	int ScalingList4x4[6][16];
	int ScalingList8x8[2][64];
}PPS;

typedef struct get_bit_context {
	uint8_t *buf;         /*指向SPS start*/
	int     buf_size;     /*SPS 长度*/
	int     bit_pos;      /*bit已读取位置*/
	int     total_bit;    /*bit总长度*/
	int     cur_bit_pos;  /*当前读取位置*/
}get_bit_context;


#define MAX_LEN 32


static int get_1bit(void *h)
{
	get_bit_context *ptr = (get_bit_context *)h;
	int ret = 0;
	uint8_t *cur_char = NULL;
	uint8_t shift;

	if (NULL == ptr)
	{
		printf( "NULL pointer");
		ret = -1;
		goto exit;
	}

	cur_char = ptr->buf + (ptr->bit_pos >> 3);
	shift = 7 - (ptr->cur_bit_pos);
	ptr->bit_pos++;
	ptr->cur_bit_pos = ptr->bit_pos & 0x7;
	ret = ((*cur_char) >> shift) & 0x01;

exit:
	return ret;
}

static int get_bits(void *h, int n)
{
	get_bit_context *ptr = (get_bit_context *)h;
	uint8_t temp[5] = { 0 };
	uint8_t *cur_char = NULL;
	uint8_t nbyte;
	uint8_t shift;
	uint32_t result;
	uint64_t ret = 0;

	if (NULL == ptr)
	{
		printf( "NULL pointer");
		ret = -1;
		goto exit;
	}

	if (n > MAX_LEN)
	{
		n = MAX_LEN;
	}

	if ((ptr->bit_pos + n) > ptr->total_bit)
	{
		n = ptr->total_bit - ptr->bit_pos;
	}

	cur_char = ptr->buf + (ptr->bit_pos >> 3);
	nbyte = (ptr->cur_bit_pos + n + 7) >> 3;
	shift = (8 - (ptr->cur_bit_pos + n)) & 0x07;

	if (n == MAX_LEN)
	{
		printf("n > MAX_LEN\n");
	}

	memcpy(&temp[5 - nbyte], cur_char, nbyte);
	ret = (uint32_t)temp[0] << 24;
	ret = ret << 8;
	ret = ((uint32_t)temp[1] << 24) | ((uint32_t)temp[2] << 16)\
		| ((uint32_t)temp[3] << 8) | temp[4];

	ret = (ret >> shift) & (((uint64_t)1 << n) - 1);

	result = ret;
	ptr->bit_pos += n;
	ptr->cur_bit_pos = ptr->bit_pos & 0x7;

exit:
	return result;
}

static int parse_codenum(void *buf)
{
	uint8_t leading_zero_bits = -1;
	uint8_t b;
	uint32_t code_num = 0;

	for (b = 0; !b; leading_zero_bits++)
	{
		b = get_1bit(buf);
	}

	code_num = ((uint32_t)1 << leading_zero_bits) - 1 + get_bits(buf, leading_zero_bits);

	return code_num;
}

static int parse_ue(void *buf)
{
	return parse_codenum(buf);
}

static int parse_se(void *buf)
{
	int ret = 0;
	int code_num;

	code_num = parse_codenum(buf);
	ret = (code_num + 1) >> 1;
	ret = (code_num & 0x01) ? ret : -ret;

	return ret;
}

/////
static void de_emulation_prevention(BYTE* buf, int* buf_size)
{
	int i = 0, j = 0;
	BYTE* tmp_ptr = NULL;
	unsigned int tmp_buf_size = 0;
	int val = 0;

	tmp_ptr = buf;
	tmp_buf_size = *buf_size;
	for (i = 0; i<(tmp_buf_size - 2); i++)
	{
		//check for 0x000003
		val = (tmp_ptr[i] ^ 0x00) + (tmp_ptr[i + 1] ^ 0x00) + (tmp_ptr[i + 2] ^ 0x03);
		if (val == 0)
		{
			//kick out 0x03
			for (j = i + 2; j<tmp_buf_size - 1; j++)
				tmp_ptr[j] = tmp_ptr[j + 1];

			//and so we should devrease bufsize
			(*buf_size)--;
		}
	}

	return;
}

static void get_bit_context_free(void *buf)
{
	get_bit_context *ptr = (get_bit_context *)buf;

	if (ptr)
	{
		if (ptr->buf)
		{
			free(ptr->buf);
		}

		free(ptr);
	}
}

static void *de_emulation_prevention(void *buf)
{
	get_bit_context *ptr = NULL;
	get_bit_context *buf_ptr = (get_bit_context *)buf;
	int i = 0, j = 0;
	uint8_t *tmp_ptr = NULL;
	int tmp_buf_size = 0;
	int val = 0;

	if (NULL == buf_ptr)
	{
		printf( "NULL ptr \n");
		goto exit;
	}

	ptr = (get_bit_context *)malloc(sizeof(get_bit_context));
	if (NULL == ptr)
	{
		printf( "NULL ptr\n");
		goto exit;
	}

	memcpy(ptr, buf_ptr, sizeof(get_bit_context));

	ptr->buf = (uint8_t *)malloc(ptr->buf_size);
	if (NULL == ptr->buf)
	{
		printf( "NULL ptr\n");
		goto exit;
	}

	memcpy(ptr->buf, buf_ptr->buf, buf_ptr->buf_size);

	tmp_ptr = ptr->buf;
	tmp_buf_size = ptr->buf_size;
	for (i = 0; i<(tmp_buf_size - 2); i++)
	{
		/*检测0x000003*/
		val = (tmp_ptr[i] ^ 0x00) + (tmp_ptr[i + 1] ^ 0x00) + (tmp_ptr[i + 2] ^ 0x03);
		if (val == 0)
		{
			/*剔除0x03*/
			for (j = i + 2; j<tmp_buf_size - 1; j++)
			{
				tmp_ptr[j] = tmp_ptr[j + 1];
			}

			/*相应的bufsize要减小*/
			ptr->buf_size--;
		}
	}

	/*重新计算total_bit*/
	ptr->total_bit = ptr->buf_size << 3;

	return (void *)ptr;

exit:
	get_bit_context_free(ptr);
	return NULL;
}

static int h264dec_seq_parameter_set(void *buf_ptr, SPS *sps_ptr)
{
	SPS *sps = sps_ptr;
	int ret = 0;
	int profile_idc = 0;
	int i, j, last_scale, next_scale, delta_scale;
	void *buf = NULL;

	if (NULL == buf_ptr || NULL == sps)
	{
		printf( "ERR null pointer\n");
		ret = -1;
		goto exit;
	}

	memset((void *)sps, 0, sizeof(SPS));

	buf = de_emulation_prevention(buf_ptr);
	if (NULL == buf)
	{
		printf( "ERR null pointer\n");
		ret = -1;
		goto exit;
	}

	sps->profile_idc = get_bits(buf, 8);
	sps->constraint_set0_flag = get_1bit(buf);
	sps->constraint_set1_flag = get_1bit(buf);
	sps->constraint_set2_flag = get_1bit(buf);
	sps->constraint_set3_flag = get_1bit(buf);
	sps->reserved_zero_4bits = get_bits(buf, 4);
	sps->level_idc = get_bits(buf, 8);
	sps->seq_parameter_set_id = parse_ue(buf);

	profile_idc = sps->profile_idc;// printf("--profile_idc=%d\n", profile_idc);
	if ((profile_idc == 100) || (profile_idc == 110) || (profile_idc == 122) || (profile_idc == 244)
		|| (profile_idc == 44) || (profile_idc == 83) || (profile_idc == 86) || (profile_idc == 118) || \
		(profile_idc == 128)) 

	{
		sps->chroma_format_idc = parse_ue(buf);
		if (sps->chroma_format_idc == 3)
		{
			sps->separate_colour_plane_flag = get_1bit(buf);
		}

		sps->bit_depth_luma_minus8 = parse_ue(buf);
		sps->bit_depth_chroma_minus8 = parse_ue(buf);
		sps->qpprime_y_zero_transform_bypass_flag = get_1bit(buf);

		sps->seq_scaling_matrix_present_flag = get_1bit(buf);
		if (sps->seq_scaling_matrix_present_flag)
		{
			for (i = 0; i<((sps->chroma_format_idc != 3) ? 8 : 12); i++)
			{
				sps->seq_scaling_list_present_flag[i] = get_1bit(buf);
				if (sps->seq_scaling_list_present_flag[i])
				{
					if (i<6)
					{
						for (j = 0; j<16; j++)
						{
							last_scale = 8;
							next_scale = 8;
							if (next_scale != 0)
							{
								delta_scale = parse_se(buf);
								next_scale = (last_scale + delta_scale + 256) % 256;
								sps->UseDefaultScalingMatrix4x4Flag[i] = ((j == 0) && (next_scale == 0));
							}
							sps->ScalingList4x4[i][j] = (next_scale == 0) ? last_scale : next_scale;
							last_scale = sps->ScalingList4x4[i][j];
						}
					}
					else
					{
						int ii = i - 6;
						next_scale = 8;
						last_scale = 8;
						for (j = 0; j<64; j++)
						{
							if (next_scale != 0)
							{
								delta_scale = parse_se(buf);
								next_scale = (last_scale + delta_scale + 256) % 256;
								sps->UseDefaultScalingMatrix8x8Flag[ii] = ((j == 0) && (next_scale == 0));
							}
							sps->ScalingList8x8[ii][j] = (next_scale == 0) ? last_scale : next_scale;
							last_scale = sps->ScalingList8x8[ii][j];
						}
					}
				}
			}
		}
	}

	sps->log2_max_frame_num_minus4 = parse_ue(buf);
	sps->pic_order_cnt_type = parse_ue(buf);
	if (sps->pic_order_cnt_type == 0)
	{
		sps->log2_max_pic_order_cnt_lsb_minus4 = parse_ue(buf);
	}
	else if (sps->pic_order_cnt_type == 1)
	{
		sps->delta_pic_order_always_zero_flag = get_1bit(buf);
		sps->offset_for_non_ref_pic = parse_se(buf);
		sps->offset_for_top_to_bottom_field = parse_se(buf);

		sps->num_ref_frames_in_pic_order_cnt_cycle = parse_ue(buf);
		for (i = 0; i<sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
		{
			sps->offset_for_ref_frame_array[i] = parse_se(buf);
		}
	}

	sps->num_ref_frames = parse_ue(buf);
	sps->gaps_in_frame_num_value_allowed_flag = get_1bit(buf);
	sps->pic_width_in_mbs_minus1 = parse_ue(buf);
	sps->pic_height_in_map_units_minus1 = parse_ue(buf);

	sps->frame_mbs_only_flag = get_1bit(buf);
	if (!sps->frame_mbs_only_flag)
	{
		sps->mb_adaptive_frame_field_flag = get_1bit(buf);
	}

	sps->direct_8x8_inference_flag = get_1bit(buf);

	sps->frame_cropping_flag = get_1bit(buf);
	if (sps->frame_cropping_flag)
	{
		sps->frame_crop_left_offset = parse_ue(buf);
		sps->frame_crop_right_offset = parse_ue(buf);
		sps->frame_crop_top_offset = parse_ue(buf);
		sps->frame_crop_bottom_offset = parse_ue(buf);
	}

	sps->vui_parameters_present_flag = get_1bit(buf);
	if (sps->vui_parameters_present_flag)
	{
		// 解析 VUI 有异常 
		//vui_parameters_set(buf, &sps->vui_parameters);
	}

#if SPS_PPS_DEBUG
	sps_info_print(sps);
#endif

exit:
	get_bit_context_free(buf);
	return ret;
}

static int h264_sps_parse(SPS* sps, unsigned char* sps_buffer, int sps_len)
{
	get_bit_context buffer;
	memset(&buffer, 0, sizeof(buffer));
	buffer.buf = sps_buffer + 1;
	buffer.buf_size = sps_len - 1;
	memset(sps, 0, sizeof(SPS));

	int r = h264dec_seq_parameter_set(&buffer, sps);
	
	int width = (sps->pic_width_in_mbs_minus1 + 1) * 16;
	int height = (2 - sps->frame_mbs_only_flag)* (sps->pic_height_in_map_units_minus1 + 1) * 16;

	width -= (sps->frame_crop_left_offset + sps->frame_crop_right_offset) * 2;
	height -= (sps->frame_crop_top_offset + sps->frame_crop_bottom_offset) * 2;

	///
	if (sps->frame_cropping_flag)
	{
/*		unsigned int crop_unit_x;
		unsigned int crop_unit_y;
		if (0 == sps->chroma_format_idc) // monochrome
		{
			crop_unit_x = 1;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		}
		else if (1 == sps->chroma_format_idc) // 4:2:0
		{
			crop_unit_x = 2;
			crop_unit_y = 2 * (2 - sps->frame_mbs_only_flag);
		}
		else if (2 == sps->chroma_format_idc) // 4:2:2
		{
			crop_unit_x = 2;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		}
		else // 3 == sps.chroma_format_idc   // 4:4:4
		{
			crop_unit_x = 1;
			crop_unit_y = 2 - sps->frame_mbs_only_flag;
		}

		width -= crop_unit_x * (sps->frame_crop_left_offset + sps->frame_crop_right_offset);
		height -= crop_unit_y * (sps->frame_crop_top_offset + sps->frame_crop_bottom_offset); */

		
	}

	sps->pic_width = width;
	sps->pic_height = height;

	return r;
}

