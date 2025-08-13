#ifndef REQUEST_STATUS_HPP
#define REQUEST_STATUS_HPP


enum RequestStatus {
	NEED_MORE_DATA,       
	HEADERS_ARE_READY,      
	BODY_BEING_READ,      
	EVERYTHING_IS_OK,   
	

	BAD_REQUEST = 400,          // 400 
	FORBIDDEN = 403,            // 403 
	NOT_FOUND = 404,            // 404
	METHOD_NOT_ALLOWED = 405,   // 405
	LENGTH_REQUIRED = 411,      // 411 - Missing Content-Length
	PAYLOAD_TOO_LARGE = 413,    // 413 - Request body too large
	URI_TOO_LONG = 414,         // 414 - Request-URI too long
	HEADER_TOO_LARGE = 431,     // 431 - Request header fields too large
	INTERNAL_ERROR = 500,       // 500 
	NOT_IMPLEMENTED = 501       // 501 - Method not supported
};

#endif 
