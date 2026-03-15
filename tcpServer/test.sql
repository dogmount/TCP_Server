create table if not exists t_user(
    f_id int not null auto_increment comment'自增id',
    f_user_id int not null comment'用户id',
    f_username varchar(64) not null comment'用户昵称',
    f_password varchar(64) not null comment'用户密码',
    f_custonface varchar(100) not null comment'用户头像',
    f_register_time datetime not null default current_timestamp comment'注册时间',
    f_update_time timestamp not null default current_timestamp on update current_timestamp comment'更新时间',
    primary key (f_user_id),
    key f_id (f_id)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

create table if not exists t_relationship(
    f_id int not null auto_increment comment'自增id',
    f_user_id1 int not null comment'第一个用户id',
    f_user_id2 int not null comment'第二个用户id',
    f_teamname1 varchar(64) default '我的好友' comment '用户2在用户1的好友分组名称',
    f_markname1 varchar(32) comment '用户2在用户1的备注名称',
    f_teamname2 varchar(64) default '我的好友' comment '用户1在用户2的好友分组名称',
    f_markname2 varchar(32) comment '用户1在用户2的备注名称',
    f_update_time timestamp not null default current_timestamp on update current_timestamp comment'更新时间',
    primary key (f_id)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

create table if not exists t_chatmsg(
    f_id int not null auto_increment comment'自增id',
    f_senderid int not null comment'发送者id',
    f_targetid int not null comment'接收者id',
    f_msgtype varchar(32) not null comment'消息类型：1-文本，2-图片，3-文件等',
    f_msgcontent text comment'聊天内容',
    f_url varchar(500) comment'媒体文件url',
    f_create_time timestamp not null default current_timestamp comment'创建时间',
    primary key (f_id),
    index f_senderid (f_senderid),
    key f_targetid (f_targetid)
)ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;