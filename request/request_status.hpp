#ifndef REQUEST_STATUS_HPP
#define REQUEST_STATUS_HPP


enum RequestStatus {
	NEED_MORE_DATA,       
	HEADERS_ARE_READY,      
	BODY_BEING_READ,      
	EVERYTHING_IS_OK,   
	

	BAD_REQUEST,          // 400 
	FORBIDDEN,            // 403 
	NOT_FOUND,            // 404
	METHOD_NOT_ALLOWED,   // 405
	PAYLOAD_TOO_LARGE,    // 413 
	INTERNAL_ERROR        // 500 
};

#endif 
