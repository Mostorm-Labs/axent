# Rooms协议文档

# 设备发现协议

```JSON
//请求组播设备发现   224.0.2.251:6565   
//请求消息体  Discovery
{
    "method":"DeviceDiscovery"
}


//请求响应
{
    "model":"ROOMS",       //ROOMS, AIR，DECODE，CAMERA
    "sn":"AW0BLCAB9A99000080",
    "version":"V1.0.0.0.R.240909",
    "name":"ROOMS_PLUS",
    "ipv4":"192.168.16.23",
    "protocols":[
        {
            "type":"ws",     //    http/ws， 目前只支持websocket协议，pp:私有协议
            "port":"8080"    // 50362 私有协议端口
        }
    ]
}
```



# 协议介绍

该协议作为我司设备/平台之间互联互通协议，协议内容基于json文本格式实现，提供完善的会话控制，请求/响应，事件通知等机制。且与具体网络传输层协议解耦，可使用http/websocket/udp等协议实现，目前该协议设备端支持了websocket传输。

## 通用字段介绍

### op\(报文类型\)

该字段常用值解析

|**字段值**|**值解析**|
|---|---|
|0|Hello 用于握手，创建会话|
|1|HelloAck 握手响应报文类型|
|5|Subscribe 订阅请求|
|6|Event 事件通知请求|
|7|Request 资源方法请求|
|8|RequestResponse 请求的响应报文|
|14|bye 结束会话请求报文|
|15|byeAck bye请求的响应报文|
|||

### 协议框架固定字段信息描述

|**字段**|**字段介绍**|
|---|---|
|adxdpVersion|版本信息|
|rpcVersion|版本信息|
|supportedMethods|支持的方法集合|
|supportedEvents|支持的事件集合|
|op|操作类型|
|d|请求描述体|
|method|请求方法名称|
|sid<br>|会话id,握手请求创建sid，后续所有消息都需要携带该字段|
|params|请求参数|
|id<br>|请求id,每一个method请求，都需要携带该字段，用来唯一标识一个请求，响应的报文也要携带该字段|
|status|请求响应的状态信息|
|code|错误码|
|comment|错误描述信息|
|result|请求结果信息|
|event|事件名称|
|data|事件内容信息|



### 常用错误码

```C++

    enum RequestStatus {
        /**
        * Unknown status, should never be used.
        *
        * @enumIdentifier Unknown
        * @enumValue 0
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        Unknown = 0,

        /**
        * For internal use to signify a successful field check.
        *
        * @enumIdentifier NoError
        * @enumValue 10
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        NoError = 10,

        /**
        * The requestHandler has succeeded.
        *
        * @enumIdentifier Success
        * @enumValue 100
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        Success = 100,

        /**
        * The `requestType` field is missing from the requestHandler data.
        *
        * @enumIdentifier MissingRequestType
        * @enumValue 203
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        MissingRequestType = 203,
        /**
        * The requestHandler type is invalid or does not exist.
        *
        * @enumIdentifier UnknownRequestType
        * @enumValue 204
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        UnknownRequestType = 204,
        /**
        * Generic error code.
        *
        * Note: A comment is required to be provided by obs-websocket.
        *
        * @enumIdentifier GenericError
        * @enumValue 205
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        GenericError = 205,
        /**
        * The requestHandler batch execution type is not supported.
        *
        * @enumIdentifier UnsupportedRequestBatchExecutionType
        * @enumValue 206
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        UnsupportedRequestBatchExecutionType = 206,
        /**
        * The server is not ready to handle the requestHandler.
        *
        * Note: This usually occurs during OBS scene collection change or exit. Requests may be tried again after a delay if this code is given.
        *
        * @enumIdentifier NotReady
        * @enumValue 207
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.3.0
        * @api enums
        */
        NotReady = 207,

        /**
        * A required requestHandler field is missing.
        *
        * @enumIdentifier MissingRequestField
        * @enumValue 300
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        MissingRequestField = 300,
        /**
        * The requestHandler does not have a valid requestData object.
        *
        * @enumIdentifier MissingRequestData
        * @enumValue 301
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        MissingRequestData = 301,

        /**
        * Generic invalid requestHandler field messageHandler.
        *
        * Note: A comment is required to be provided by obs-websocket.
        *
        * @enumIdentifier InvalidRequestField
        * @enumValue 400
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidRequestField = 400,
        /**
        * A requestHandler field has the wrong data type.
        *
        * @enumIdentifier InvalidRequestFieldType
        * @enumValue 401
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidRequestFieldType = 401,
        /**
        * A requestHandler field (number) is outside of the allowed range.
        *
        * @enumIdentifier RequestFieldOutOfRange
        * @enumValue 402
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        RequestFieldOutOfRange = 402,
        /**
        * A requestHandler field (string or array) is empty and cannot be.
        *
        * @enumIdentifier RequestFieldEmpty
        * @enumValue 403
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        RequestFieldEmpty = 403,
        /**
        * There are too many requestHandler fields (eg. a requestHandler takes two optionals, where only one is allowed at a time).
        *
        * @enumIdentifier TooManyRequestFields
        * @enumValue 404
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        TooManyRequestFields = 404,

        /**
        * An output is running and cannot be in order to perform the requestHandler.
        *
        * @enumIdentifier OutputRunning
        * @enumValue 500
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        OutputRunning = 500,
        /**
        * An output is not running and should be.
        *
        * @enumIdentifier OutputNotRunning
        * @enumValue 501
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        OutputNotRunning = 501,
        /**
        * An output is paused and should not be.
        *
        * @enumIdentifier OutputPaused
        * @enumValue 502
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        OutputPaused = 502,
        /**
        * An output is not paused and should be.
        *
        * @enumIdentifier OutputNotPaused
        * @enumValue 503
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        OutputNotPaused = 503,
        /**
        * An output is disabled and should not be.
        *
        * @enumIdentifier OutputDisabled
        * @enumValue 504
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        OutputDisabled = 504,
        /**
        * Studio mode is active and cannot be.
        *
        * @enumIdentifier StudioModeActive
        * @enumValue 505
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        StudioModeActive = 505,
        /**
        * Studio mode is not active and should be.
        *
        * @enumIdentifier StudioModeNotActive
        * @enumValue 506
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        StudioModeNotActive = 506,

        /**
        * The resource was not found.
        *
        * Note: Resources are any kind of object in obs-websocket, like inputs, profiles, outputs, etc.
        *
        * @enumIdentifier ResourceNotFound
        * @enumValue 600
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        ResourceNotFound = 600,
        /**
        * The resource already exists.
        *
        * @enumIdentifier ResourceAlreadyExists
        * @enumValue 601
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        ResourceAlreadyExists = 601,
        /**
        * The type of resource found is invalid.
        *
        * @enumIdentifier InvalidResourceType
        * @enumValue 602
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidResourceType = 602,
        /**
        * There are not enough instances of the resource in order to perform the requestHandler.
        *
        * @enumIdentifier NotEnoughResources
        * @enumValue 603
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        NotEnoughResources = 603,
        /**
        * The state of the resource is invalid. For example, if the resource is blocked from being accessed.
        *
        * @enumIdentifier InvalidResourceState
        * @enumValue 604
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidResourceState = 604,
        /**
        * The specified input (obs_source_t-OBS_SOURCE_TYPE_INPUT) had the wrong kind.
        *
        * @enumIdentifier InvalidInputKind
        * @enumValue 605
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidInputKind = 605,
        /**
        * The resource does not support being configured.
        *
        * This is particularly relevant to transitions, where they do not always have changeable settings.
        *
        * @enumIdentifier ResourceNotConfigurable
        * @enumValue 606
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        ResourceNotConfigurable = 606,
        /**
        * The specified filter (obs_source_t-OBS_SOURCE_TYPE_FILTER) had the wrong kind.
        *
        * @enumIdentifier InvalidFilterKind
        * @enumValue 607
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        InvalidFilterKind = 607,

        /**
        * Creating the resource failed.
        *
        * @enumIdentifier ResourceCreationFailed
        * @enumValue 700
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        ResourceCreationFailed = 700,
        /**
        * Performing an action on the resource failed.
        *
        * @enumIdentifier ResourceActionFailed
        * @enumValue 701
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        ResourceActionFailed = 701,
        /**
        * Processing the requestHandler failed unexpectedly.
        *
        * Note: A comment is required to be provided by obs-websocket.
        *
        * @enumIdentifier RequestProcessingFailed
        * @enumValue 702
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        RequestProcessingFailed = 702,
        /**
        * The combination of requestHandler fields cannot be used to perform an action.
        *
        * @enumIdentifier CannotAct
        * @enumValue 703
        * @enumType RequestStatus
        * @rpcVersion -1
        * @initialVersion 5.0.0
        * @api enums
        */
        CannotAct = 703,
    };
```





# 设备连接握手创建会话



客户端`发起握手协议`，将版本信息/支持的方法/支持的事件告诉对端，对端创建会话，生成全局唯一的sid。

```JSON
//请求握手连接
{
    "d": {
        "adxdpVersion": "5.2.3",   
        "rpcVersion": 1,
        "supportedMethods": [
            "GetVersion",
            "StartAgentReboot",
            "KeepAlive",
            "StartDeviceReboot",
            "StartAgentUpgrade",
            "GetDeviceList"
        ],
        "supportedEvents": [
            "EventUpdateDevice",
            "EventUpgradeProgress"
        ]
    },
    "op": 0
}


//握手响应
{
    "d": {
        "negotiatedRpcVersion": 1,
        "expireIn": 300,
        "supportedMethods": [
            "GetVersion",
            "StartAgentReboot",
            "KeepAlive",
            "StartDeviceReboot",
            "StartAgentUpgrade",
            "GetDeviceList"
        ],
        "supportedEvents": [
            "EventUpdateDevice",
            "EventUpgradeProgress"
        ]
    },
    "op": 1,
    "sid": "123456"    //后续所有请求消息都要携带sid
}




```



# 设备连接断开挥手销毁会话

```JSON
//请求销毁会话
{
    "d": {},
    "op": 14,
    "sid": "1234567"
}


//销毁会话响应
{
    "d": {},
    "op": 15,
    "sid": "1234567"
}

```



# 会话协议保活\-KeepAlive

```JSON
//保活请求
{
    "d": {
        "id": "ab22-dddaa",      //请求id
        "method": "KeepAlive"
    },
    "op": 7,                   
    "sid": "123456"
}


//保活响应
{
    "d": {
        "id": "ab22-dddaa",
        "method": "KeepAlive",
        "status": {
            "code": 100,
            "result": true
        }
    },
    "op": 8,
    "sid": 123456
}

```

# 输入源管理

## 创建输入源\-CreateInputSource

```JSON
//输入源取流地址规则   rtsp://[ip]:554/input/[inputSourceId]
//创建输入源，外部只支持创建Network类型的输入源
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateInputSource",
        "params":{
                "name":"V520D-V1",
                "type":"Network",
                "streamContent":"rtsp://192.168.16.8/mainstream"
        }
    }
}


//正常响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateInputSource",
        "status": {
        
        
                 "code": 100,
                 "result": true
        },
        "result":{
                "inputSourceId"：6,
                "name":"V520D-V1"
        }
    }
}


//异常响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateInputSource",
        "status": {
                 "code": 205,
                 "result": false,
                 "comment":"the inputSource. has existed"
        }
    }
}
```



## 获取输入源列表\-GetInputSource

```JSON


//获取输入源 请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetInputSource",
        "params":{}
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
        "d":{
            "id": "ab22-dddaa",
            "method":"GetInputSource",
            "status": {
                     "code": 100,
                     "result": true
            },
            "result":{
                "sourceList":[
                    {
                            "name":"USB-1",            //输入源名称
                            "type":"USB-1",            //输入源接口类型  USB-1/USB-2/HDMI-IN/LINE-IN 固定输入源类型
                            "ability":"video/audio",      //输入源流能力集  video(只有视频数据), audio(只有音频数据), video/audio(音视频混合数据) 0x
                            "streamContent":"",        //流描述信息                                     //输入源流描述
                            "inputSourceId"：1,            //输入源唯一id，后续使用都用id来操作
                            "status":true             //输入源接口是否正常连接设备，可拉流
                    },
                    {
                            "name":"USB-2",
                            "type":"USB-2",
                            "ability":"video/audio",
                            "streamContent":"",
                            "inputSourceId"：2,
                            "status":false
                    },
                    {
                            "name":"HDMI-IN",
                            "type":"HDMI-IN",
                            "ability":"video/audio",
                            "streamContent":"",
                            "inputSourceId"：3,
                            "status":true
                    },
                    {
                            "name":"LINE-IN",
                            "type":"LINE-IN",
                            "ability":"audio",
                            "streamContent":"",
                            "inputSourceId"：4,
                            "status":true
                    },
                    {
                            "name":"Network-1",
                            "type":"Network",
                            "ability":"video/audio",
                            "streamContent":"rtsp://192.168.16.12/live/mainstream",
                            "inputSourceId"：5,
                            "status":false    //未开启场景，网络流该状态始终为false
                    }
                ]
            }
        }
}
```

## 设置输入源信息\-SetInputSource

```JSON

//修改输入源，外部只支持修改Network类型的输入源
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetInputSource"
        "params":{
            "inputSourceId"：6
            "streamContent":"rtsp://192.168.16.12/live/mainstream"    //支持设置输入源的content信息/name信息
            "name":"Network-1",
        }
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetInputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetInputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the input source not exist"
        }
    }
}
```

## 删除输入源\-DeleteInputSource

```JSON

//删除输入源，外部只支持删除Network类型的输入源

{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteInputSource"
        "params":{
              "inputSourceId"：6
        }
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteInputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteInputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the inputSource not exist"
        }
    }
}
```



## 开启网络输入源拉流\-OpenNetInputSource

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"OpenNetInputSource"
        "params":{
              "inputSourceId"：6
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"OpenNetInputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}


//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"OpenNetInputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the inputSource not exist"
        }
    }
}

```



## 关闭网络输入源拉流\-CloseNetInputSource

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CloseNetInputSource"
        "params":{
              "inputSourceId"：6
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CloseNetInputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}


//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CloseNetInputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the inputSource not exist"
        }
    }
}
```







# 输出源管理

## 获取输出源列表\-GetOutputSource

```JSON


//获取输出源 请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetOutputSource",
        "params":{
                
        }
    }
}


//响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetOutputSource",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "sourceList":[
                {
                    "name":"USB-1",
                    "type":"USB-1",            //输出源接口类型  USB-1/USB-2/HDMI-OUT/USB-C/LINE-OUT 固定输出源类型
                    "ability":"audio",      //输出源流数据类型  video(只有视频数据), audio(只有音频数据), video/audio(音视频混合数据)
                    "streamContent":"",     //输出源流描述
                    "outputSourceId":1,       //输出源唯一标识id，后续都用该字段操作输出源
                    "status":true             //输出源接口是否正常连接设备，可拉流
                },
                {
                    "name":"USB-2",
                    "type":"USB-2",
                    "ability":"audio",
                    "streamContent":"",
                    "outputSourceId":2,
                    "status":false
                },
                {
                    "name":"USB-C",
                    "type":"USB-C",
                    "ability":"video/audio",
                    "streamContent":"",
                    "outputSourceId":3,
                    "status":false
                },
                {
                    "name":"HDMI-OUT",
                    "type":"HDMI-OUT",
                    "ability":"video/audio",
                    "streamContent":"",
                    "outputSourceId":4,
                    "status":true
                },
                {
                    "name":"LINE-OUT",
                    "type":"LINE-OUT",
                    "ability":"audio",
                    "streamContent":"",
                    "outputSourceId":5,
                    "status":true 
                },
                {
                    "name":"Network-1",
                    "type":"Network",
                    "ability":"video/audio",
                    "streamContent":"rtmp://192.168.16.100/live/mainstream",
                    "outputSourceId":6,
                    "status":false    //网络输出流固定为false
                }
            ]
        }
    }
}
```

## 创建输出源\-CreateOutputSource

```JSON

//创建输出源
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateOutputSource",
        "params":{
            "type":"Network", //目前只支持网络类型的输出源
            "streamContent":"rtmp://192.168.16.12/live/mainstream",  
            "name":"Network-1"
        }
    }
}

//创建成功响应
{
    "op": 8,
    "sid":"123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateOutputSource",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "outSourceId"：6,
            "name":"Network-1"
        }
    }
}

//创建失败响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateOutputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the outSource has existed"
        }
    }
}
```

## 删除输出源\-DeleteOutputSource

```JSON

//删除输出源，外部只支持删除Network类型的输出源

{
    "op": 7,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteOutputSource",
        "params":{
            "outputSourceId"：6
        }
    }
}


//响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteOutputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//错误响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteOutputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the inputSource not exist"
        }
    }
}
```

## 设置输出源信息\-SetOutputSource

```JSON

//修改输入源，外部只支持修改Network类型的输出源
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetOutputSource",
        "params":{
            "outputSourceId"：6,
            "streamContent":"rtsp://192.168.16.12/live/mainstream",    //支持设置输入源的content信息/name信息
            "name":"Network-1",
        }
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetOutputSource",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetOutputSource",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the input source not exist"
        }
    }
}
```



# ~~视频拼接器管理~~

## ~~创建视频拼接器\-CreateVideoMixer~~

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateVideoMixer",
        "params":{
            "name":"recordMixer",
            "videoMixer":{
                "detail":[
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":0,              //画面左上角坐标[x,y]
                            "y":0,
                            "z":0            //z标识画面层数
                        },
                        "inputSourceId":1,     //通过GetInputSource获取，输入源
                    },
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":1920,           //画面左上角坐标[x,y]
                            "y":0，
                            "z":0
                        },
                        "inputSourceId":6,     //通过GetInputSource获取
                    }
                ]
            }
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateVideoMixer",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "videoMixerId"：6,
            "name":"recordMixer"
        }
    }
}

```

## ~~设置视频拼接器\-SetVideoMixer~~

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetVideoMixer",
        "params":{
            "videoMixerId"：6, //必填字段
            "name":"recordMixer",
            "videoMixer":{
                "detail":[
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":0,              //画面左上角坐标[x,y]
                            "y":0,
                            "z":0            //z标识画面层数
                        },
                        "inputSourceId":1,     //通过GetInputSource获取，输入源
                    },
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":1920,           //画面左上角坐标[x,y]
                            "y":0，
                            "z":0
                        },
                        "inputSourceId":6,     //通过GetInputSource获取
                    }
                ]
            }
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetVideoMixer",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

```

## ~~删除视频拼接器\-DeleteVideoMixer~~

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteVideoMixer",
        "params":{
            "videoMixerId"：6, //必填字段
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteVideoMixer",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

```

## ~~获取视频拼接器信息\-GetVideoMixer~~

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetVideoMixer",
        "params":{
            //"videoMixerId"：6, //不填获取所有拼接器
        }
    }
}

//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateVideoMixer",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "videoMixerList":[
                {
                    "videoMixerId"：6, //必填字段
                    "name":"recordMixer",
                    "videoMixer":{
                        "detail":[
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":0,              //画面左上角坐标[x,y]
                                    "y":0,
                                    "z":0            //z标识画面层数
                                },
                                "inputSourceId":1,     //通过GetInputSource获取，输入源
                            },
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":1920,           //画面左上角坐标[x,y]
                                    "y":0，
                                    "z":0
                                },
                                "inputSourceId":6,     //通过GetInputSource获取
                            }
                        ]
                    }
                },
                {
                    "videoMixerId"：7, //必填字段
                    "name":"hdmiOutMixer",
                    "videoMixer":{
                        "detail":[
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":0,              //画面左上角坐标[x,y]
                                    "y":0,
                                    "z":0            //z标识画面层数
                                },
                                "inputSourceId":1,     //通过GetInputSource获取，输入源
                            },
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":1920,           //画面左上角坐标[x,y]
                                    "y":0，
                                    "z":0
                                },
                                "inputSourceId":6,     //通过GetInputSource获取
                            }
                        ]
                    }
                }
            ]
        }
    }
}
```



# 简单场景管理

## 创建场景\-CreateScene

```JSON
//场景取流地址  rtsp://[ip]:554/scene/[sceneId]
//创建场景
{
    "op": 7,
    
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateScene",
        "params":{
            "name":"classRoom-1",      //场景名称
            "enable":true,                      //场景创建成功后，是否自动开启
            "outputSourceList":[    //一个拼接/混音支持多个输出源输出
                {
                    "enable":true,
                    "outputSourceId": 4,     //通过GetOutputSource获取，输出源
                    "videoInfo":{                 //可选字段
                        "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                        "fps":30,                     //输出帧率   25/30/60     //可选 
                        "kbps":4096,                  //输出码率                                //可选
                        "format":"H264",             //视频编码  //  H264/H265   //可选
                    },
                    "audioInfo":{               //可选字段
                        "channelNum":2,         //输出音频通道个数
                        "sampleRate":48000, //输出音频采样率  8000/44100/48000
                        "bitDepth":16,      //输出音频位深    8/16/24/32
                        "format":"AAC",         //输出音频编码    PCM/AAC
                        "gain":100
                    }
                },
                {
                    "enable":true,
                    "outputSourceId": 6,
                    "videoInfo":{                     //可选字段
                        "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                        "fps":30,                  //输出帧率   25/30/60     //可选 
                        "kbps":4096,               //输出码率                                //可选
                        "format":"H264",            //视频编码  //  H264/H265   //可选
                    },
                    "audioInfo":{             //可选字段
                        "channelNum":2,         //输出音频通道个数
                        "sampleRate":48000,    //输出音频采样率  8000/44100/48000
                        "bitDepth":16,         //输出音频位深    8/16/24/32
                        "format":"AAC",        //输出音频编码    PCM/AAC
                        "gain":100
                    }
                }
            ],
            "videoMixer":{ 
                "detail":[
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":0,              //画面左上角坐标[x,y]
                            "y":0,
                            "z":0            //z标识画面层数
                        },
                        "inputSourceId":1,     //通过GetInputSource获取，输入源
                    },
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":1920,           //画面左上角坐标[x,y]
                            "y":0，
                            "z":0
                        },
                        "inputSourceId":6,     //通过GetInputSource获取
                    }
                ]
            
            },
            "audioMixer":{                                                        
                "detail":[
                    {
                        "enable":true,
                        "gain":30,     //音频轨音量增益 0~100
                        "inputSourceId":5,
                        "delay" : 100
                    },
                    {
                        "enable":true,
                        "gain":80,     //音频轨音量增益 0~100
                        "inputSourceId":3,
                        "delay" : 0
                    }
                ]
            }                        
        }
    }
}

//创建成功响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateScene",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "sceneId":1
        }
    }
}

//创建失败响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CreateScene",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the inputSourceId  2 not exist"
        }
    }
}

```



## 获取场景信息\-GetScene

```JSON

//获取场景布局配置
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetScene"
        "params":{
            //"sceneId":1   //过滤场景ID，不填则获取所有场景配置
            //"enable":true //过滤当前处于开启状态的场景
        }
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetScene",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "sceneList":[
                {
                    "sceneId":1
                    "name":"classRoom-1",      //场景名称
                    "enable":true,                      //场景是否处于启动状态
                    "outputSourceList":[    //一个拼接/混音支持多个输出源输出
                        {
                                "outputSourceId": 4,     //通过GetOutputSource获取，输出源
                                "videoInfo":{                                //可选字段
                                    "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                                    "fps":30,                //输出帧率   25/30/60     //可选 
                                    "kbps":4096,             //输出码率                                //可选
                                    "format":"H264"       //视频编码  //  H264/H265   //可选
                                },
                                "audioInfo":{                        //可选字段
                                    "channelNum":2,         //输出音频通道个数
                                    "sampleRate":48000, //输出音频采样率  8000/44100/48000
                                    "bitDepth":16,      //输出音频位深    8/16/24/32
                                    "format":"AAC",         //输出音频编码    PCM/AAC
                                    "gain":100
                                }
                        },
                        {
                                "outputSourceId": 6,
                                "videoInfo":{                                //可选字段
                                    "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                                    "fps":30,                                        //输出帧率   25/30/60     //可选 
                                    "kbps":4096,                                //输出码率                                //可选
                                    "format":"H264"                        //视频编码  //  H264/H265   //可选
                                },
                                "audioInfo":{                        //可选字段
                                    "channelNum":2,         //输出音频通道个数
                                    "sampleRate":48000, //输出音频采样率  8000/44100/48000
                                    "bitDepth":16,      //输出音频位深    8/16/24/32
                                    "format":"AAC",        //输出音频编码    PCM/AAC
                                    "gain":100
                                }

                        }
                    ],    
                    //"videoMixerId":9, //指定videoMixerId， 通过CreateVideoMixer创建
                    "videoMixer":{
                        "detail":[
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":0,                                //画面左上角坐标[x,y]
                                    "y":0,
                                    "z":0
                                },
                                "inputSourceId":1,     //通过GetInputSource获取，输入源
                            },
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":1920,                                //画面左上角坐标[x,y]
                                    "y":0，
                                    "z":0
                                },
                                "inputSourceId":6,     //通过GetInputSource获取
                            }
                        ]
                    
                    },
                    "audioMixer":{                                                        
                        "detail":[
                            {
                                "enable":true,   //是否加入混音
                                "gain":30,     //音频轨音量增益 0~100
                                "inputSourceId":5,
                                "delay" : 0  //单位ms
                            },
                            {
                                "enable":true,  //是否加入混音
                                "gain":80,     //音频轨音量增益 0~100
                                "inputSourceId":3,
                                "delay" : 0  //单位ms
                            }
                        ]
                    }
                },
                
                {
                    "sceneId":2
                    "name":"classRoom-2",      //场景名称
                    "enable":false,                      //场景是否处于启动状态
                    "outputSourceList":[    //一个拼接/混音支持多个输出源输出
                        {
                            "outputSourceId": 7,     //通过GetOutputSource获取，输出源
                            "videoInfo":{                                //可选字段
                                "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                                "fps":30,                                        //输出帧率   25/30/60     //可选 
                                "kbps":4096,                                //输出码率                                //可选
                                "format":"H264"                        //视频编码  //  H264/H265   //可选
                            },
                            "audioInfo":{                        //可选字段
                                "channelNum":2,         //输出音频通道个数
                                "sampleRate":48000, //输出音频采样率  8000/44100/48000
                                "bitDepth":16,      //输出音频位深    8/16/24/32
                                "format":"AAC",         //输出音频编码    PCM/AAC
                                "gain":100
                            }
                        },
                        {
                            "outputSourceId":8,
                            "videoInfo":{                                //可选字段
                                "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                                "fps":30,                                        //输出帧率   25/30/60     //可选 
                                "kbps":4096,                                //输出码率                                //可选
                                "format":"H264",                        //视频编码  //  H264/H265   //可选
                                "gain":100
                            },
                            "audioInfo":{                        //可选字段
                                "channelNum":2,         //输出音频通道个数
                                "sampleRate":48000, //输出音频采样率  8000/44100/48000
                                "bitDepth":16,      //输出音频位深    8/16/24/32
                                "format":"AAC",        //输出音频编码    PCM/AAC
                                "gain":100
                            }
                        }
                    ],    
                    "videoMixer":{
                        "detail":[
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":0,                                //画面左上角坐标[x,y]
                                    "y":0,
                                    "z":0
                                },
                                "inputSourceId":1     //通过GetInputSource获取，输入源
                            },
                            {
                                "position":{          //以8K分辨率为基准画布  7680×4320
                                    "width":1920,       //画面宽度
                                    "height":1080,      //画面高度
                                    "x":1920,                                //画面左上角坐标[x,y]
                                    "y":0，
                                    "z":0
                                },
                                "inputSourceId":6     //通过GetInputSource获取
                            }
                        ]
                    },
                    "audioMixer":{                                                        
                        "detail":[
                            {
                                "enable":true,
                                "gain":30,     //音频轨音量增益 0~100
                                "inputSourceId":5,
                                "delay" : 0  //单位ms
                            },
                            {
                                "enable":true,
                                "gain":80,     //音频轨音量增益 0~100
                                "inputSourceId":3,
                                "delay" : 0  //单位ms
                            }
                        ]
                    }
                }
            ]
        }
    }
}



```



## 获取场景输出配置信息\-GetSceneOutConfig

```JSON
{
    "op": 7,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSceneOutConfig",
        "params":{
            "sceneId":2,         //必填字段  场景id
            "outputSourceId":3      //可选字段， 不填返回所有输出源信息
        }
    }
}

{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSceneOutConfig",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
             "outputSourceList":[
                {
                        "outputSourceId": 3,     //通过GetOutputSource获取，输出源
                        "name":"HDMI-OUT",
                        "videoInfo":{                                //可选字段
                            "resolution":"1080p",         //支持1080p/720p/360p    //可选字段
                            "fps":30,                //输出帧率   25/30/60     //可选 
                            "kbps":4096,             //输出码率                                //可选
                            "format":"H264"       //视频编码  //  H264/H265   //可选
                        },
                        "audioInfo":{                        //可选字段
                            "channelNum":2,         //输出音频通道个数
                            "sampleRate":48000, //输出音频采样率  8000/44100/48000
                            "bitDepth":16,      //输出音频位深    8/16/24/32
                            "format":"AAC",         //输出音频编码    PCM/AAC
                            "gain":100
                        }
                }
             ]    
         }
    }
}



```





## 获取场景混音器信息\-GetAudioMixerItem

```JSON
{
    "op": 7,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAudioMixerItem",
        "params":{
            "sceneId":2,         //必填字段  场景id
            "inputSourceId":3      //可选字段， 不填返回混音器所有信息
        }
    }
}

{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAudioMixerItem",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
             "detail":[
                    {
                        "enable":true,   //是否加入混音
                        "gain":30,     //音频轨音量增益 0~100
                        "inputSourceId":3,
                        "delay": 100,
                        "name": "LINE-IN"
                    }
                ]    
         }
    }
}

```



## 设备默认场景配置

音频路由场景配置

|输入\\输出|USB\-1\-OUT|USB\-2\-OUT|USB\-C\-OUT|HDMI\-OUT|LINE\-OUT|Rec/Stream|
|---|---|---|---|---|---|---|
|USB\-1\-IN||* [x] |* [x] |* [x] |* [x] |* [x] |
|USB\-2\-IN||* [x] |* [x] |* [x] |* [x] |* [x] |
|USB\-C\-IN|* [x] |||* [x] |* [x] |* [x] |
|HDMI\-IN||* [x] |* [x] |* [x] |* [x] |* [x] |
|Line\-IN||* [x] |* [x] |* [x] |* [x] |* [x] |

每一个音频输出都是单独的场景，可单独配置里面的混音器







## 配置场景混音器信息\-SetAudioMixerItem

```JSON
{
    "op": 7,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"*SetAudioMixerItem*",
        "params":{
            "sceneId":2,         //必填字段  场景id
            "action":"set",      //  set（设置音频条目信息）/add（添加音频条目信息）/remove（删除音频条目信息）
            "audioMixerDetail":{
                "enable":true,  //是否加入混音
                "gain":30,     //音频轨音量增益 0~100
                "inputSourceId":5,
                "delay" : 0  //单位ms
            }            
        }
    }
}

{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAudioMixerItem",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
        
         }
    }
}


{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAudioMixerItem",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}

```





## 设置场景信息\-SetScene

```JSON

//设置场景信息
//修改设置场景名称
{
    "op": 7,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "params":{
            "sceneId":2, //必填字段
            "name":"room-3"
        }
    }
}

//修改场景子单元拼接信息, 更新outputSourceList， 其他字段类比修改即可
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "params":{
            "sceneId":2, //必填字段
            "outputSourceList":[    //一个拼接/混音支持多个输出源输出
                {
                    "outputSourceId": 4,     //通过GetOutputSource获取，输出源
                    "audioInfo":{               //可选字段
                        "channelNum":2,         //输出音频通道个数
                        "sampleRate":48000, //输出音频采样率  8000/44100/48000
                        "bitDepth":16,      //输出音频位深    8/16/24/32
                        "format":"AAC",         //输出音频编码    PCM/AAC
                        "gain":100
                    }
                },
                {
                    "outputSourceId" : 12,
                    "videoInfo" : 
                    {
                            "format" : "H264",
                            "fps" : 30,
                            "kbps" : 4096,
                            "resolution" : "1080p"
                    }
                }
            ]
        }
    }
}


//修改场景混音信息, 全量更新audioMixer， 其他字段类比修改即可
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "params":{
            "sceneId":2, //必填字段
            "audioMixer":{                                                        
                "detail":[
                    {
                        "enable":true,   //是否加入混音  
                        "gain":30,     //音频轨音量增益 0~100
                        "inputSourceId":5,
                        "delay" : 0  //单位ms
                    },
                    {
                        "enable":true,  //是否加入混音
                        "gain":80,     //音频轨音量增益 0~100
                        "inputSourceId":3,
                        "delay" : 0  //单位ms
                    }
                ]
            }
        }
    }
}


//修改场景子单元拼接信息, 全量更新videoMixer， 其他字段类比修改即可
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "params":{
            "name":"roomsPlus",
            "sceneId":2, //必填字段
            "videoMixer":{
                "detail":[
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":0,                                //画面左上角坐标[x,y]
                            "y":0,
                            "z":0
                        },
                        "inputSourceId":1     //通过GetInputSource获取，输入源
                    },
                    {
                        "position":{          //以8K分辨率为基准画布  7680×4320
                            "width":1920,       //画面宽度
                            "height":1080,      //画面高度
                            "x":1920,                                //画面左上角坐标[x,y]
                            "y":0，
                            "z":0
                        },
                        "inputSourceId":6     //通过GetInputSource获取
                    }
                ]
            }
        }
    }
}




//设置成功响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
                
        }
    }
}

//设置失败响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetScene",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}
```



## 删除场景\-DeleteScene

```JSON

//删除输出源，外部只支持删除Network类型的输出源
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteScene"
        "params":{
            "sceneId"：2
        }
    }
}


//响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteScene",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//错误响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"DeleteScene",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}
```



## 开启场景\-StartScene

```JSON

//开启场景
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StartScene",
        "params":{
            "sceneId":2  //必填字段
        }
    }
}


//开启场景成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartScene",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//开启场景失败响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartScene",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}
```



## 停止场景\-StopScene

```JSON

//停止场景
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StopScene",
        "params":{
            "sceneId":2 //必填字段
        }
    }
}


//开启场景成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StopScene",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//开启场景失败响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StopScene",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}
```



## 开启场景录制\-StartSceneRecord

```JSON

//开启场景录制
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StartSceneRecord",
        "params":{
            "sceneId":2  //必填字段
        }
    }
}


//开启场景录制成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartSceneRecord",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//开启场景录制失败响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartSceneRecord",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartSceneRecord",
        "status": {
             "code": 205,
             "result": false,
             "comment":"not found sd"
        }
    }
}
```

## 停止场景录制\-StopSceneRecord

```JSON

//停止场景录制
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StopSceneRecord",
        "params":{
            "sceneId":2  //必填字段
        }
    }
}


//停止场景录制成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StopSceneRecord",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//开启场景录制失败响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StopSceneRecord",
        "status": {
             "code": 205,
             "result": false,
             "comment":"the sceneId not exist"
        }
    }
}
```



## 录像查询\-SelectRecord

```JSON
//录像查询
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SelectRecord",
        "params":{
        }
    }
}


{
    "op": 8,
    "sid": "123456",
        "d":{
            "id": "ab22-dddaa",
            "method":"SelectRecord",
            "status": {
                     "code": 100,
                     "result": true
            },
            "result":{
                "recordList":[
                    {
                        "url":"http://192.168.16.10/RecordFile/1/2024-11-08/10-00-00.mp4"
                    },
                    {
                        "url":"http://192.168.16.10/RecordFile/1/2024-11-07/11-10-00.mp4"
                    }
                ]
            }
        }
}
```

## 录像删除\-RemoveRecord

```JSON
//录像删除请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"RemoveRecord",
        "params":{
            "recordList":[
                    {
                        "url":"http://192.168.16.10/RecordFile/1/2024-11-08/10-00-00.mp4"
                    },
                    {
                        "url":"http://192.168.16.10/RecordFile/1/2024-11-07/11-10-00.mp4"
                    }
                ]
        }
    }
}

//录像删除响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"RemoveRecord",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

```



## 开启场景直播推流\-StartLiveStream

```JSON

//开启场景直播推流请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StartLiveStream",
        "params":{
            "sceneId":2,  //必填字段
            "url":"rtmp://192.168.11.1/live/stream" ////必填字段
        }
    }
}

//响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StartLiveStream",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



## 停止场景直播推流\-StopLiveStream

```JSON
//请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"StopLiveStream",
        "params":{
            "sceneId":1  //必填字段
        }
    }
}

//响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"StopLiveStream",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



# 视频拼接切换业务管理

## 设置工作场景（录制流）视频输出模式 SetWorkSceneOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetWorkSceneOutPlayMode",
        "params":{
            // Presentation/Manual/AutoDirector 
            // [Presentation演讲者模式，有分享流（hdmi-in/tail投屏流）进来，分享流与其他视频源自动布局] [Manual 自定义视频拼接]
            // [AutoDirector] 需要配置波束映射，根据波束位置自动切换画面
            "mode":"Manual"
          }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetWorkSceneOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



## 获取工作场景（录制流）视频输出模式 GetWorkSceneOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetWorkSceneOutPlayMode",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetWorkSceneOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            // Presentation/Manual/AutoDirector 
            // [Presentation演讲者模式，有分享流（hdmi-in/tail投屏流）进来，分享流与其他视频源自动布局] [Manual 自定义视频拼接]
            // [AutoDirector] 需要配置波束映射，根据波束位置自动切换画面
            "mode":"Manual"
        }
    }
}

```





## 设置HDMI\-OUT视频输出模式  SetHdmiOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHdmiOutPlayMode",
        "params":{
            // Share/Preview/Manual [Share模式，有分享流进来hdmiOut只显示分享流, 无分享流显示向导页面]  
            // [Preview 模式跟随录制场景拼接信息]   [Manual 模式自定义布局显示]
            "mode":"Manual"
          }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHdmiOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



## 获取HDMI\-OUT视频输出模式  GetHdmiOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetHdmiOutPlayMode",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetHdmiOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            // Share/Preview/Manual [Share模式，有分享流进来hdmiOut只显示分享流, 无分享流显示向导页面]  
            // [Preview 模式跟随录制场景拼接信息]   [Manual 模式自定义布局显示]
            "mode":"Manual"
        }
    }
}

```

## 设置HDMI\-OUT投屏share输出模式 SetHdmioutShareMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHdmioutShareMode",
        "params":{
                        // 0: tail和Hdmi in相互覆盖, 
                        //1: tail优先级最高，拔掉tail后如果Hdmi in有画面，Hdmi in画面会显示在引导图片上, 
                        //2: Hdmi in不参与
            "mode":0
          }
    }
}

{
    "d": {
        "id": "ab22-dddaa",
        "method": "SetHdmioutShareMode",
        "status": {
            "code": 100,
            "result": true
        }
    },
    "op": 8,
    "sid": "18446744072144177456"
}
```

## 获取HDMI\-OUT投屏share输出模式  GetHdmioutShareMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetHdmioutShareMode",
        "params":{
          }
    }
}

{
    "d": {
        "id": "ab22-dddaa",
        "method": "GetHdmioutShareMode",
        "result": {
            "mode": 2
        },
        "status": {
            "code": 100,
            "result": true
        }
    },
    "op": 8,
    "sid": "18446744072144478824"
}
```

## 设置TYPEC\-OUT视频输出模式  SetTypecOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetTypecOutPlayMode",
        "params":{
            // Auto/Preview/Manual [Auto模式，使用除去HDMI投屏内容的自动布局画面]    [Manual 模式自定义布局显示]
            // [Preview 模式跟随录制场景拼接信息]   [custom 模式自定义布局显示]
            "mode":"Manual"
          }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetTypecOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 获取TYPEC\-OUT视频输出模式  GetTypecOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetTypecOutPlayMode",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetTypecOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            // Auto/Preview/Manual [Auto模式，使用除去HDMI投屏内容的自动布局画面]    [Manual 模式自定义布局显示]
            // [Preview 模式跟随录制场景拼接信息]   [custom 模式自定义布局显示]
            "mode":"Manual"
        }
    }
}

```



## 设置BYOM\-OUT视频输出模式  SetByomOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetByomOutPlayMode",
        "params":{
            // Auto/Preview/Manual [Auto模式，使用除去Tail投屏内容的自动布局画面]    [Manual 模式自定义布局显示]
            // [Preview 模式跟随录制场景拼接信息] 
            "mode":"Manual"
          }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetByomOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 获取BYOM\-OUT视频输出模式  GetByomOutPlayMode

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetByomOutPlayMode",
        "params":{
          }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetByomOutPlayMode",
        "status": {
             "code": 100,
             "result": true
        }，
        "result":{
            // Auto/Preview/Manual [Auto模式，使用除去Tail投屏内容的自动布局画面]    [Manual 模式自定义布局显示]
            // [Preview 模式跟随录制场景拼接信息] 
            "mode":"Manual"
        }
    }
}
```

# AMX100\+A50 波束联动方案配置

## AMX100连接\-AMX100Connect

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100Connect",
        "params":{
            "ipaddr":"xxx.xxx.xxx.xxx",
            "port":5022
         }
     }
}


//正常连接成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100Connect",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//连接失败响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100Connect",
        "status": {
             "code": 205,
             "result": false,
             "comment":"AMX100 connect failed"
        }
    }
}
```

## AMX100断开连接\-AMX100DisConnect

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100DisConnect",
        "params":{
            "ipaddr":"xxx.xxx.xxx.xxx",
            "port":5022
         }
     }
}


//正常连接成功响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100DisConnect",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//连接失败响应
{
    "op": 8,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"AMX100DisConnect",
        "status": {
             "code": 205,
             "result": false,
             "comment":"AMX100 DisConnect failed"
        }
    }
}
```

## 获取AMX100子设备及波束信息\-GetAMX100DeviceInfo

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAMX100DeviceInfo",
        "params":{
            
         }
     }
}


{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAMX100DeviceInfo",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "devices":[
                {
                    "model":"A50",
                    "sn":"AW31YCAB899A000560",
                    "beams":12
                },
                {
                    "model":"A50",
                    "sn":"AW31YCAB899A000491",
                    "beams":12
                }
            ]
         }
     }
}


```

## \-\-

## 配置音频导播模式波束映射关系\-SetAutoDirectorBeamMapConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d": {
        "id": "ab22-dddaa",
        "method": "SetAutoDirectorBeamMapConfig",
        "params": {
            "sn": "AW31YCAB899A000491",
            "model": "a40", //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
            "devIndex": 0, //int   ,amx100级联设备的索引号，从0开始
            "beamIndex": 1, //int, 波束索引号 从1开始
            "name": "USB-1",  //唯一标识videoMixer
            "presetPosition": -1,  //预置位，
            "videoMixer": {
                "detail": [
                    {
                        "position": { //以8K分辨率为基准画布  7680×4320
                            "width": 1920, //画面宽度
                            "height": 1080, //画面高度
                            "x": 0, //画面左上角坐标[x,y]
                            "y": 0,
                            "z": 0 //z标识画面层数
                        },
                        "inputSourceId": 1, //通过GetInputSource获取，输入源
                    },
                    {
                        "position": { //以8K分辨率为基准画布  7680×4320
                            "width": 1920, //画面宽度
                            "height": 1080, //画面高度
                            "x": 1920, //画面左上角坐标[x,y]
                            "y": 0，
                            "z": 0
                        },
                        "inputSourceId": 6, //通过GetInputSource获取
                    }
                ]
            }
        }
    }
}



{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetBeamMapConfig",
        "status": {
             "code": 100,
             "result": true
        }
     }
}
```

## 获取音频导播模式波束映射关系\-GetAutoDirectorBeamMapConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAutoDirectorBeamMapConfig",
        "params":{
         }
     }
}



{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAutoDirectorBeamMapConfig",
        "status": {
             "code": 100,
             "result": true
        },
        "result": {
            "beamStatistics":10,      //波束统计缓存个数
            "beamTriggerCount":5,    //触发视频拼接转换的最大波束个数
            "autoDirectorBeamMaps": [
                {
                    "sn": "AW31YCAB899A000491",
                    "model": "a40", //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
                    "devIndex": 0, //int   ,amx100级联设备的索引号 从0开始
                    "beams":12, //波束个数
                    "maps":[
                        {
                            "name":"USB-1",
                             "presetPosition": -1, 
                            "beamIndex": 2
                        },
                        {
                            "name":"USB-2",
                            "presetPosition": -1,
                            "beamIndex": 1
                        }
                    ]
                },
                {
                    "sn": "AW31YCAB899A000491",
                    "model": "a40", //string，amx100设备级联的设备类型 "am10P","a30","a40","a50"    
                    "devIndex": 1, //int   ,amx100级联设备的索引号 从0开始
                    "beams":12, //波束个数
                    "maps":[
                        {
                            "name":"USB-1",
                            "presetPosition": -1,
                            "beamIndex": 2
                        },
                        {
                            "name":"USB-2",
                            "presetPosition": -1,
                            "beamIndex": 1
                        },
                        
                    ]
                }
            ]
        }
    }
}
```

## ~~设置音频导播模式波束统计信息SetAutoDirectorBeamStatisticsInfo \-\-\-\-调试使用~~

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"~~SetAutoDirectorBeamStatisticsInfo~~",
        "params":{    //字段定义暂时先这样做
            "autoDirectorBeamStatistics":10,      //波束统计缓存个数
            "autoDirectorBeamTriggerCount":5    //触发视频拼接转换的最大波束个数
         }
     }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"~~SetAutoDirectorBeamStatisticsInfo~~",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## \-\-

## 配置讨论者高亮配置波束映射关系\-SetSpeakerHighlightBeamMapConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d": {
        "id": "ab22-dddaa",
        "method": "SetSpeakerHighlightBeamMapConfig",
        "params": {
            "sn": "AW31YCAB899A000491",
            "model": "a40", //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
            "devIndex": 0, //int   ,amx100级联设备的索引号，从0开始
            "beamIndex": 1, //int, 波束索引号 从1开始
            "inputSourceId": 1,  //int, 所绑定的单源id
            
        }
    }
}



{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSpeakerHighlightBeamMapConfig",
        "status": {
             "code": 100,
             "result": true
        }
     }
}
```

## 获取讨论者高亮配置波束映射关系\-GetSpeakerHighlightBeamMapConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSpeakerHighlightBeamMapConfig",
        "params":{
         }
     }
}

{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSpeakerHighlightBeamMapConfig",
        "status": {
             "code": 100,
             "result": true
        },
        "result": {
            "beamStatistics":10,      //波束统计缓存个数
            "beamTriggerCount":5,    //触发视频拼接转换的最大波束个数
            "speakerHighlightBeamMaps": [
                {
                    "sn": "AW31YCAB899A000491",
                    "model": "a40", //string，amx100设备级联的设备类型 "am10P","a30","a40","a50","a50P"   
                    "devIndex": 0, //int   ,amx100级联设备的索引号 从0开始
                    "beams":12, //波束个数
                    "maps":[
                        {
                            "inputSourceId":1,
                            "beamIndex": 2
                        },
                        {
                            "inputSourceId":4,
                            "beamIndex": 1
                        }
                    ]
                },
                {
                    "sn": "AW31YCAB899A000491",
                    "model": "a40", //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
                    "devIndex": 1, //int   ,amx100级联设备的索引号 从0开始
                    "beams":12, //波束个数
                    "maps":[
                        {
                            "inputSourceId":6,
                            "beamIndex": 2
                        },
                        {
                            "inputSourceId":3,
                            "beamIndex": 1
                        },    
                    ]
                }
            ]
        }
    }
}
```

## 获取讨论者高亮配置开关\-GetSpeakerHighlightConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSpeakerHighlightConfig",
        "params":{
         }
     }
}

{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSpeakerHighlightConfig",
        "status": {
             "code": 100,
             "result": true
        },
        "result": {
            "enable":true,      //配置是否开启
        }
    }
}
```

## 设置讨论者高亮配置开关\-SetSpeakerHighlightConfig

```json
{
    "op": 7,
    "sid": "123456",
    "d": {
        "id": "ab22-dddaa",
        "method": "SetSpeakerHighlightConfig",
        "params": {
            "enable" : true
        }
    }
}



{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSpeakerHighlightConfig",
        "status": {
             "code": 100,
             "result": true
        }
     }
}
```

## ~~设置讨论者高亮波束统计信息SetSpeakerHighlightBeamStatisticsInfo \-\-\-\-调试使用~~

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"~~SetSpeakerHighlightBeamStatisticsInfo~~",
        "params":{      //字段定义暂时先这样做
            "speakerHighlightBeamStatistics":10,      //波束统计缓存个数
            "speakerHighlightBeamTriggerCount":5    //触发视频拼接转换的最大波束个数
         }
     }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"~~SetSpeakerHighlightBeamStatisticsInfo~~",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

# 子设备控制

## 子设备信息获取

```JSON
待定

```

# 事件订阅

## 输入源/输出源状态订阅

```JSON
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"InputSourceChange"   //OutputSourceChange
    }
}

//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"InputSourceChange",      //OutputSourceChange
        "data":{
            "name":"USB-2",
            "type":"USB-2",
            "ability":"video/audio",
            "streamContent":"",
            "inputSourceId"：2,
            "status":false，
        }
    }
}


```

## 推流直播状态订阅

```JSON
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"LiveStreamStatus"
    }
}



//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"LiveStreamStatus",     
        "data":{
            "sceneId":1,
            "url":"rtmp://192.168.11.3/live/stream",
            //推流成功【streaming】， 推流失败【failed】， 正在重连【reconnecting】
            //推流停止【stopped】
            "liveStatus":"streaming",
            "liveSeconds":100
        }
    }
}


```

## SD卡存储状态订阅

```JSON
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"SDStatus"
    }
}


//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"SDStatus",     
        "data":{
            //sdAddUnformatted sd卡插入未格式化
            //sdAddFormatted   sd卡插入已格式化
            //sdRemove    sd卡拔掉
            //sdFull   sd卡空间已满
            //sdFormatSuccess
            //sdFormatFail
            "status":"sdAddUnformatted",
            //sd卡总空间
            "totalSize":2344444,
            //sd卡可用空间
            "availableSize":100000
        }
    }
}


```

## 录制流状态订阅

```JSON
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"RecordStatus"
    }
}

//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"RecordStatus",     
        "data":{
            //recording    正在录制
            //recordStopped 录制停止
            "status":"recording",
            "recordSeconds":100
        }
    }
}


```



## USB\-C 连接状态订阅

```json
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"UsbcConnectEvent"
    }
}

//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"UsbcConnectEvent",     
        "data":{
            //usbcConnect usb-c已连接
            //usbcDisConnect usb-c断开连接
            "status":"usbcConnect"
        }
    }
}
```

## USB\-C 视频拉流状态订阅

```json
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"UsbcStreamEvent"
    }
}

//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"UsbcStreamEvent",     
        "data":{
            //usbcStartVideoStream   usb-c视频拉流
            //usbcStopVideoStream     usb-c停止视频拉流
            "status":"usbcStopVideoStream"
        }
    }
}
```



## AMX100连接状态订阅

```json
//订阅请求
{
    "op": 5,
    "sid": "123456",
    "d":{
        "subscribe":"AMX100ConnectStatusEvent"
    }
}


//事件通知
{
    "op":6,
    "sid":"123456",
    "d":{
        "event":"AMX100ConnectStatusEvent",     
        "data":{
            //AMX100Connected   已连接状态
            //AMX100DisConnected  断开连接状态
            "status":"AMX100Connected"
        }
    }
}
```

# 设置系统时间\-SetSysTime

```JSON
//设置系统时间请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSysTime",
        "params":{
//时区   America/New_York   America/Los_Angeles  Europe/Berlin Europe/Moscow Asia/Kolkata Asia/Shanghai Asia/Tokyo Australia/Sydney  Pacific/Auckland
            "timezone": "Asia/Shanghai",
            "year": 2024,         
            "month": 11,         
            "day": 12,
            "hour": 14,         
            "minute": 30,
            "second": 45
        }
    }
}

//设置系统时间响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSysTime",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

```



# 格式化SD卡\-FormatSd

```JSON
//格式化sd卡 请求
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"FormatSd",
        "params":{
        }
    }
}

//格式化一般比较耗时，这里设备端基本都是返回成功，通过订阅事件通知格式化结果
//格式化sd卡 响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"FormatSd",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//格式化失败响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"FormatSd",
        "status": {
             "code": 205,
             "result": false,
             "comment":"device may be busy, please stop record"
        }
    }
}
```



# 获取SD卡存储信息\-GetSDInfo

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSDInfo",
        "params":{
        }
    }
}


{
    "op": 8,
    "sid": "123456",
        "d":{
            "id": "ab22-dddaa",
            "method":"GetSDInfo",
            "status": {
                 "code": 100,
                 "result": true
            },
            "result":{
                //sdAddUnformatted sd卡插入未格式化
                //sdAddFormatted   sd卡插入已格式化
                //sdRemove    sd卡拔掉
                "status":"sdAddUnformatted",
                //sd卡总空间
                "totalSize":2344444,
                //sd卡可用空间
                "availableSize":100000
            }
        }
}



```

# 重置配置\-ResetConfig

```JSON
//重置配置后，设备会自动重启

{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"ResetConfig",
        "params":{
          }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"ResetConfig",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



# 获取设备基本信息\-GetDeviceInfo

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetDeviceInfo",
        "params":{
         }
    }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetDeviceInfo",
        "status": {
             "code": 100,
             "result": true
        }，
        "result":{
            "model":"ROOMS",       //设备型号
            "devName":"classRooms-201",        //设备名称
            "cpuUsage":60,       //已使用CPU百分比
            "memoryUsage":20,    //已使用内存百分比
            "ip":"192.168.19.165",   //设备有线网卡地址
            "version":"V1.0.0.0.R.250120"，   //设备固件版本
            "mac":"98:6E:E8:50:03:A1"  //设备mac地址
        }
    }
}

```



# 设置设备名称\-SetDeviceName

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetDeviceName",
        "params":{
            "devName":"classRooms-201"
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetDeviceName",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



# KVM 数据传输

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetKvm",
        "params":{
            "kvmData":"1267abdc"
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetKvm",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "kvmData":"1267abdc"
        }
    }
}
```

# 远程升级

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"RemoteUpgrade",
        "params":{
            "url":"http://fgsy.uuc.com/file/General_MB_[Nearhub_ROOMS]_V1.0.0.0.R.250304.bin"
         }
    }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"RemoteUpgrade",
        "status": {
             "code": 100,
             "result": true
        }
    }
}


```

# 远程升级进度查询

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"UpgradeProgress",
        "params":{
            "url":"http://fgsy.uuc.com/file/General_MB_[Nearhub_ROOMS]_V1.0.0.0.R.250304.bin"
         }
    }
}


{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"UpgradeProgress",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "progress":20   //0~100
        }
    }
}
```



# 设置Line\-in预增益

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetLineInPreGain",
        "params":{
            "preGain":10  //1~10
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetLineInPreGain",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



# 获取Line\-in预增益

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetLineInPreGain",
        "params":{
           
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetLineInPreGain",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "preGain":1  //1~10
        }
    }
}
```



# 设置音频播放模式

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAudioPlaybackMode",
        "params":{
            "mode":"priority"     //or manual
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAudioPlaybackMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

# 获取音频播放模式

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAudioPlaybackMode",
        "params":{
           
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAudioPlaybackMode",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "mode":"priority"     //or manual
        }
    }
}
```



# 获取USB\-C连接状态

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetUsbcConnectStatus",
        "params":{
           
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetUsbcConnectStatus",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            //usbcConnect usb-c已连接
            //usbcDisConnect usb-c断开连接
            "status":"usbcConnect"
        }
    }
}

```



# 

# V520D/V540D支持协议

## 设置智能模式

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAlgMode",
        "params":{
            "mode":"Manual"    //Manual AutoFraming SmartTracking SpeakerTacking
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetAlgMode",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 获取智能模式

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAlgMode",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetAlgMode",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "mode":"Manual"    //Manual AutoFraming SmartTracking SpeakerTacking
        }
    }
}
```



## 设置DOA Mic源

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetDoaMic",
        "params":{
            "mic":"IntMic"    //IntMic ExtMic
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetDoaMic",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 获取DOA Mic源

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetDoaMic",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetDoaMic",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "mic":"IntMic"    //IntMic ExtMic
        }
    }
}
```



## 简化版本

由于目前520d/540d的相关基础库版本很老，没法直接更新到最新，故没法使用ws，故添加一个基于tcp的连接，camera端为server，b10端为client，端口号：50362,报文格式为json

### 获取设备信息

```JSON
{
    "op": 7,
    "method":"GetDevInfo",
    "params":{
    }         
}

{
    "op": 8,
    "method":"GetDevInfo",
    "status": {
         "code": 100,
         "result": true
    }
    "result":{
        "devType":"V520D",    //V403 V410 V415 V520D V540D
        "algCapacity":1111    //分别对应Manual AutoFraming SmartTracking SpeakerTracking，比如支持Manual AutoFraming，则值为1100
    }
}
```



### 设置智能模式

```JSON
{
    "op": 7,
    "method":"SetAlgMode",
    "params":{
        "mode":"Manual"    //Manual AutoFraming SmartTracking SpeakerTracking
     }
}

//正常响应
{
    "op": 8,
    "method":"SetAlgMode",
    "status": {
         "code": 100,
         "result": true
    }
}

//异常响应
{
    "op": 8,
    "method":"SetAlgMode",
    "status": {
             "code": 205,
             "result": false,
             "comment":"Fail to SetAlgMode"
    }
}
```

### 获取智能模式

```JSON
{
    "op": 7,
    "method":"GetAlgMode",
    "params":{
    }         
}

{
    "op": 8,
    "method":"GetAlgMode",
    "status": {
         "code": 100,
         "result": true
    }
    "result":{
        "mode":"Manual"    //Manual AutoFraming SmartTracking SpeakerTracking
    }
}
```



### 设置DOA Mic源

```JSON
{
    "op": 7,
    "method":"SetDoaMic",
    "params":{
        "mic":"IntMic"    //IntMic ExtMic
     }
}

//正常响应
{
    "op": 8,
    "method":"SetDoaMic",
    "status": {
         "code": 100,
         "result": true
    }
}

//异常响应
{
    "op": 8,
    "method":"SetDoaMic",
    "status": {
             "code": 205,
             "result": false,
             "comment":"Fail to SetDoaMic"
    }
}
```

### 获取DOA Mic源

```JSON
{
    "op": 7,
    "method":"GetDoaMic",
    "params":{
    }
}

{
    "op": 8,
    "method":"GetDoaMic",
    "status": {
         "code": 100,
         "result": true
    }
    "result":{
        "mic":"IntMic"    //IntMic ExtMic
    }
}
```

### 设置跟AMX100联动参数

```JSON
{
    "op": 7,
    "method":"SetAMX100Param",
    "params":{
        "amx100Dev":[{
             "devType": "a40",  //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
             "index": 0,        //int   ,amx100级联设备的索引号
             "snid": "88888888", //string,amx100级联设备的sn码
             "beam":[-1,-1,3,-1,-1,-1,-1,-1] //array, 数组大小根据设备不同会改变，a50:12,a40:8,amp10:1,amx100级联设备的波束对应的预置位信息(-1表示无预置位，可支持的预置位为0~255)
              },
              {
             "devType": "a40",  //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
             "index": 1,        //int   ,amx100级联设备的索引号
             "snid": "88888888", //string,amx100级联设备的sn码
             "beam":[-1,-1,3,-1,-1,-1,-1,-1] //array, 数组大小根据设备不同会改变，a50:12,a40:8,amp10:1,amx100级联设备的波束对应的预置位信息(-1表示无预置位，可支持的预置位为0~255)
              }]
    }
}

{
    "op": 8,
    "method":"SetAMX100Param",
    "status": {
         "code": 100,
         "result": true
    }
}
```

### 获取跟AMX100联动参数

```JSON
{
    "op": 7,
    "method":"GetAMX100Param",
    "params":{
    }
}

{
    "op": 8,
    "method":"GetAMX100Param",
    "status": {
         "code": 100,
         "result": true
    }
    "result":{
        "amx100Dev":[{
             "devType": "a40",  //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
             "index": 0,        //int   ,amx100级联设备的索引号
             "snid": "88888888", //string,amx100级联设备的sn码
             "beam":[-1,-1,3,-1,-1,-1,-1,-1] //array, 数组大小根据设备不同会改变，a50:12,a40:8,amp10:1,amx100级联设备的波束对应的预置位信息(-1表示无预置位，可支持的预置位为0~255)
              },
              {
             "devType": "a40",  //string，amx100设备级联的设备类型 "am10p","a30","a40","a50"    
             "index": 1,        //int   ,amx100级联设备的索引号
             "snid": "88888888", //string,amx100级联设备的sn码
             "beam":[-1,-1,3,-1,-1,-1,-1,-1] //array, 数组大小根据设备不同会改变，a50:12,a40:8,amp10:1,amx100级联设备的波束对应的预置位信息(-1表示无预置位，可支持的预置位为0~255)
              }]
    }
}
```

### 设置AMX100的ip地址

```JSON
{
    "op": 7,
    "method":"SetAMX100Ip",
    "params":{
        "ipaddr":"xxx.xxx.xxx.xxx" 
     }
}

//正常响应
{
    "op": 8,
    "method":"SetAMX100Ip",
    "status": {
         "code": 100,
         "result": true
    }
}

//异常响应
{
    "op": 8,
    "method":"SetAMX100Ip",
    "status": {
             "code": 205,
             "result": false,
             "comment":"Fail to SetAMX100Ip"
    }
}
```

### 获取AMX100的ip地址

```JSON
{
    "op": 7,
    "method":"GetAMX100Ip",
    "params":{
    }
}

//正常响应
{
    "op": 8,
    "method":"GetAMX100Ip",
    "status": {
         "code": 100,
         "result": true
    }
    "result":{
        "ipaddr":"xxx.xxx.xxx.xxx"
    }
}

//异常响应
{
    "op": 8,
    "method":"GetAMX100Ip",
    "status": {
             "code": 205,
             "result": false,
             "comment":"Fail to GetAMX100Ip"
    }
}
```

# 设置Line Out音量

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetLineOutVolume",
        "params":{
            "volume":10  //0~16，0:mute
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetLineOutVolume",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```



# 获取Line Out音量

```json
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetLineOutVolume",
        "params":{
           
         }
    }
}



{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetLineOutVolume",
        "status": {
             "code": 100,
             "result": true
        },
        "result":{
            "volume":10  //0~16，0:mute
        }
    }
}
```

# Decode产测协议

## 连接wifi

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"WifiConnect",
        "params":{
            "ssid":"wifi name",
            "key":"wifi key"
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"WifiConnect",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"WifiConnect",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## 获取已连接wifi的信号强度

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetWifiSignalStrength",
        "params":{
        }        
    } 
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetWifiSignalStrength",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "signalStrength":50        //0--100
        }
    }
}
```



## 视频播放

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"VideoPlay",
        "params":{
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"VideoPlay",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 音频播放

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"AudioPlay",
        "params":{
         }
    }
}

{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"AudioPlay",
        "status": {
             "code": 100,
             "result": true
        }
    }
}
```

## 检测line port状态

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"CheckLineStatus",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"CheckLineStatus",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "lineInStatus":1,
            "lineOutStatus":1
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"CheckLineStatus",
        "status": {
             "code": 205,
             "result": false
        }
         "result":{
            "lineInStatus":1,
            "lineOutStatus":1
        }
    }
}
```

## kvm测试

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"TestKvm",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"TestKvm",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"TestKvm",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## USB口测试

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"TestUsb",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"TestUsb",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"TestUsb",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```



## 设置sn码

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSn",
        "params":{
            "snSerial":"xxxxxx"
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSn",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetSn",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## 获取sn码

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSn",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSn",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "snSerial":"xxxxxx"
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetSn",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```



## 设置mac地址

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetMacAddr",
        "params":{
            "mac":"xx:xx:xx:xx:xx:xx"
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetMacAddr",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetMacAddr",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## 获取mac地址

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"GetMacAddr",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetMacAddr",
        "status": {
             "code": 100,
             "result": true
        }
        "result":{
            "mac":"xx:xx:xx:xx:xx:xx"
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"GetMacAddr",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```



## 检测复位按键

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"ResetButton",
        "params":{
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"ResetButton",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"ResetButton",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## 设置hwid

```JSON
{
    "op": 7,
    "sid": "123456",
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHwid",
        "params":{
            "hwid":"xxxxxx"
         }
    }
}

//正常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHwid",
        "status": {
             "code": 100,
             "result": true
        }
    }
}

//异常响应
{
    "op": 8,
    "sid": 123456,
    "d":{
        "id": "ab22-dddaa",
        "method":"SetHwid",
        "status": {
             "code": 205,
             "result": false
        }
    }
}
```

## 

