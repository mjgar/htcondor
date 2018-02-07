#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_utils/file_transfer_stats.h"
#include "curl_plugin.h"
#include "utc_time.h"
#include <iostream>

#define MAX_RETRY_ATTEMPTS 20

using namespace std;

CurlPlugin::CurlPlugin( int diagnostic ) :
    _diagnostic ( diagnostic )
{
    // Initialize win32 + SSL socket libraries.
    // Do not initialize these separately! Doing so causes https:// transfers
    // to segfault.
    int init = curl_global_init( CURL_GLOBAL_DEFAULT );
    if ( init != 0 ) {
        fprintf( stderr, "Error: curl_plugin initialization failed with error"
                                                " code %d\n", init ); 
    }
    if ( ( _handle = curl_easy_init() ) == NULL ) {
        fprintf( stderr, "Error: failed to initialize CurlPlugin._handle, exiting" );
    }
}

CurlPlugin::~CurlPlugin() {
    curl_easy_cleanup( _handle );
    curl_global_cleanup();
}

/*
    Perform the curl request, and output the results either to file to stdout.
*/
int 
CurlPlugin::DownloadFile( const char* url, const char* download_file_name ) {

    char error_buffer[CURL_ERROR_SIZE];
    char partial_range[20];
    double bytes_downloaded;    
    double transfer_connection_time;
    double transfer_total_time;
    FILE *file = NULL;
    long return_code;
    int rval = -1;
    static int partial_file = 0;
    static long partial_bytes = 0;


    int close_output = 1;
    if ( !strcmp( download_file_name, "-" ) ) {
        file = stdout;
        close_output = 0;
        if ( _diagnostic ) { 
            fprintf( stderr, "fetching %s to stdout\n", url ); 
        }
    } 
    else {
        file = partial_file ? fopen( download_file_name, "a+" ) : fopen( download_file_name, "w" ); 
        close_output = 1;
        if ( _diagnostic ) { 
            fprintf( stderr, "fetching %s to %s\n", url, download_file_name ); 
        }
    }

    if( file ) {
        // Libcurl options that apply to all transfer protocols
        curl_easy_setopt( _handle, CURLOPT_URL, url );
        curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
        curl_easy_setopt( _handle, CURLOPT_WRITEDATA, file );

        // Libcurl options for HTTP, HTTPS and FILE
        if( !strncasecmp( url, "http://", 7 ) || 
                            !strncasecmp( url, "https://", 8 ) || 
                            !strncasecmp( url, "file://", 7 ) ) {
            curl_easy_setopt( _handle, CURLOPT_FOLLOWLOCATION, 1 );
            curl_easy_setopt( _handle, CURLOPT_HEADERFUNCTION, this->header_callback );
        }
        // Libcurl options for FTP
        else if( !strncasecmp( url, "ftp://", 6 ) ) {
            curl_easy_setopt( _handle, CURLOPT_WRITEFUNCTION, this->ftp_write_callback );
        }

        // * If the following option is set to 0, then curl_easy_perform()
        // returns 0 even on errors (404, 500, etc.) So we can't identify
        // some failures. I don't think it's supposed to do that?
        // * If the following option is set to 1, then something else bad
        // happens? 500 errors fail before we see HTTP headers but I don't
        // think that's a big deal.
        // * Let's keep it set to 1 for now.
        curl_easy_setopt( _handle, CURLOPT_FAILONERROR, 1 );
        
        if( _diagnostic ) {
            curl_easy_setopt( _handle, CURLOPT_VERBOSE, 1 );
        }

        // If we are attempting to resume a download, set additional flags
        if( partial_file ) {
            sprintf( partial_range, "%lu-", partial_bytes );
            curl_easy_setopt( _handle, CURLOPT_RANGE, partial_range );
        }

        // Setup a buffer to store error messages. For debug use.
        error_buffer[0] = 0;
        curl_easy_setopt( _handle, CURLOPT_ERRORBUFFER, error_buffer ); 

        // Does curl protect against redirect loops otherwise?  It's
        // unclear how to tune this constant.
        // curl_easy_setopt(_handle, CURLOPT_MAXREDIRS, 1000);
        
        // Update some statistics
        _file_transfer_stats->TransferType = "download";
        _file_transfer_stats->TransferTries += 1;

        // Perform the curl request
        rval = curl_easy_perform( _handle );

        // Check if the request completed partially. If so, set some
        // variables so we can attempt a resume on the next try.
        if( rval == CURLE_PARTIAL_FILE ) {
            if( server_supports_resume( url ) ) {
                partial_file = 1;
                partial_bytes = ftell( file );
            }
        }

        // Gather more statistics
        curl_easy_getinfo( _handle, CURLINFO_SIZE_DOWNLOAD, &bytes_downloaded );
        curl_easy_getinfo( _handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
        curl_easy_getinfo( _handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
        curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &return_code );
        
        _file_transfer_stats->TransferTotalBytes += ( long ) bytes_downloaded;
        _file_transfer_stats->ConnectionTimeSeconds +=  ( transfer_total_time - transfer_connection_time );
        _file_transfer_stats->TransferReturnCode = return_code;

        if( rval == CURLE_OK ) {
            _file_transfer_stats->TransferSuccess = true;
            _file_transfer_stats->TransferError = "";
            _file_transfer_stats->TransferFileBytes = ftell( file );
        }
        else {
            _file_transfer_stats->TransferSuccess = false;
            _file_transfer_stats->TransferError = error_buffer;
        }

        // Error handling and cleanup
        if( _diagnostic && rval ) {
            fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n", 
                    rval, curl_easy_strerror( ( CURLcode ) rval ) ); 
        }
        if( close_output ) {
            fclose( file ); 
            file = NULL;
        }
    }
    else {
        fprintf( stderr, "ERROR: could not open output file %s, error %d (%s)\n", download_file_name, errno, strerror(errno) ); 
    }

    return rval;
}

int
CurlPlugin::DownloadMultipleFiles( string input_filename ) {

    classad::ClassAd stats_ad;
    CondorClassAdFileIterator adFileIter;
    FILE* input_file;
    map<MyString, transfer_request> requested_files;
    MyString stats_string;
    int retry_count = 0;
    int rval = 0;
    UtcTime time;
    
    // Read input file containing data about files we want to transfer. Input
    // data is formatted as a series of classads, each with an arbitrary number
    // of inputs.
    input_file = safe_fopen_wrapper( input_filename.c_str(), "r" );
    if( input_file == NULL ) {
        fprintf( stderr, "Unable to open curl_plugin input file %s.\n", 
            input_filename.c_str() );
        return -1;
    }
    
    if( !adFileIter.begin( input_file, false, CondorClassAdFileParseHelper::Parse_new )) {
		fprintf( stderr, "Failed to start parsing classad input.\n" );
		return -1;
    } 
    else {
        // Iterate over the classads in the file, and insert each one into our
        // requested_files map, with the key: url, value: additional details 
        // about the transfer.
        ClassAd transfer_file_ad;
        MyString download_file_name;
        MyString url;
        transfer_request request_details;
        std::pair<MyString, transfer_request> this_request;
        
        while ( adFileIter.next( transfer_file_ad ) > 0 ) {
            transfer_file_ad.LookupString( "DownloadFileName", download_file_name );
            transfer_file_ad.LookupString( "Url", url );
            request_details.download_file_name = download_file_name;
            this_request = std::make_pair( url, request_details );
            //printf("[main] inserting url=%s, download_file_name=%s\n", url.Value(), request_details.download_file_name.Value());
            requested_files.insert( this_request );
        }
    }
    fclose(input_file);

    // Iterate over the map of files to transfer.
    for ( std::map<MyString, transfer_request>::iterator it = requested_files.begin(); it != requested_files.end(); ++it ) {

        MyString download_file_name = it->second.download_file_name;
        MyString url = it->first;

        // Initialize the stats structure for this transfer.
        _file_transfer_stats = new FileTransferStats();
        _global_ft_stats = _file_transfer_stats;
        init_stats( (char*)url.Value() );
        _file_transfer_stats->TransferStartTime = time.getTimeDouble();
        
        // Enter the loop that will attempt/retry the curl request
        for ( ;; ) {
    
            // The sleep function is defined differently in Windows and Linux
            #ifdef WIN32
                Sleep( ( retry_count++ ) * 1000 );
            #else
                sleep( retry_count++ );
            #endif
            
            rval = DownloadFile( url.Value(), download_file_name.Value() );

            // If curl request is successful, break out of the loop
            if( rval == CURLE_OK ) {    
                break;
            }
            // If we have not exceeded the maximum number of retries, and we encounter
            // a non-fatal error, stay in the loop and try again
            else if( retry_count <= MAX_RETRY_ATTEMPTS && 
                                    ( rval == CURLE_COULDNT_CONNECT ||
                                        rval == CURLE_PARTIAL_FILE || 
                                        rval == CURLE_READ_ERROR || 
                                        rval == CURLE_OPERATION_TIMEDOUT || 
                                        rval == CURLE_SEND_ERROR || 
                                        rval == CURLE_RECV_ERROR ) ) {
                continue;
            }
            // On fatal errors, break out of the loop
            else {
                break;
            }
        }

        _file_transfer_stats->TransferEndTime = time.getTimeDouble();

        // If the transfer was successful, append the stats 
        if( rval != -1 ) {
            _file_transfer_stats->Publish( stats_ad );
            sPrintAd( stats_string, stats_ad );
            _all_stats += "[\n";
            _all_stats += stats_string.Value();
            _all_stats += "]\n";
            stats_ad.Clear();
            stats_string = "";
        }
    }

    return rval;
}

int 
CurlPlugin::UploadFile( const char* url, const char* download_file_name ) {

    char error_buffer[CURL_ERROR_SIZE];
    double bytes_uploaded; 
    double transfer_connection_time;
    double transfer_total_time;
    FILE *file = NULL;
    long return_code;
    int rval = -1;

    // Output transfer: file -> URL
    int close_input = 1;
    int content_length = 0;
    struct stat file_info;

    if ( !strcmp( url, "-" ) ) {
        fprintf( stderr, "ERROR: must provide a filename for curl_plugin uploads\n" ); 
        exit(1);
    } 

    // Verify that the specified file exists, and check its content length 
    file = fopen( url, "r" );
    if( !file ) {
        fprintf( stderr, "ERROR: File %s could not be opened\n", url );
        exit(1);
    }
    if( fstat( fileno( file ), &file_info ) == -1 ) {
        fprintf( stderr, "ERROR: fstat failed to read file %s\n", url );
        exit(1);
    }
    content_length = file_info.st_size;
    close_input = 1;
    if ( _diagnostic ) { 
        fprintf( stderr, "sending %s to %s\n", url, download_file_name ); 
    }

    // Set curl upload options
    curl_easy_setopt( _handle, CURLOPT_URL, url );
    curl_easy_setopt( _handle, CURLOPT_UPLOAD, 1 );
    curl_easy_setopt( _handle, CURLOPT_READDATA, file );
    curl_easy_setopt( _handle, CURLOPT_FOLLOWLOCATION, -1 );
    curl_easy_setopt( _handle, CURLOPT_INFILESIZE_LARGE, 
                                    (curl_off_t) content_length );
    curl_easy_setopt( _handle, CURLOPT_FAILONERROR, 1 );
    if( _diagnostic ) {
        curl_easy_setopt( _handle, CURLOPT_VERBOSE, 1 );
    }

    // Does curl protect against redirect loops otherwise?  It's
    // unclear how to tune this constant.
    // curl_easy_setopt(_handle, CURLOPT_MAXREDIRS, 1000);

    // Gather some statistics
    _file_transfer_stats->TransferType = "upload";
    _file_transfer_stats->TransferTries += 1;

    // Perform the curl request
    rval = curl_easy_perform( _handle );

    // Gather more statistics
    curl_easy_getinfo( _handle, CURLINFO_SIZE_UPLOAD, &bytes_uploaded );
    curl_easy_getinfo( _handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
    curl_easy_getinfo( _handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
    curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &return_code );

    _file_transfer_stats->TransferTotalBytes += ( long ) bytes_uploaded;
    _file_transfer_stats->ConnectionTimeSeconds += transfer_total_time - transfer_connection_time;
    _file_transfer_stats->TransferReturnCode = return_code;

    if( rval == CURLE_OK ) {
        _file_transfer_stats->TransferSuccess = true;    
        _file_transfer_stats->TransferError = "";
        _file_transfer_stats->TransferFileBytes = ftell( file );
    }
    else {
        _file_transfer_stats->TransferSuccess = false;
        _file_transfer_stats->TransferError = error_buffer;
    }

    // Error handling and cleanup
    if ( _diagnostic && rval ) {
        fprintf( stderr, "curl_easy_perform returned CURLcode %d: %s\n",
                rval, curl_easy_strerror( ( CURLcode )rval ) );
    }
    if ( close_input ) {
        fclose( file ); 
        file = NULL;
    }


    return rval;    // 0 on success
}

/*
    Check if this server supports resume requests using the HTTP "Range" header
    by sending a Range request and checking the return code. Code 206 means
    resume is supported, code 200 means not supported. 
    Return 1 if resume is supported, 0 if not.
*/
int 
CurlPlugin::server_supports_resume( const char* url ) {

    int rval = -1;

    // Send a basic request, with Range set to a null range
    curl_easy_setopt( _handle, CURLOPT_URL, url );
    curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
    curl_easy_setopt( _handle, CURLOPT_RANGE, "0-0" );

    rval = curl_easy_perform(_handle);

    // Check the HTTP status code that was returned
    if( rval == 0 ) {
        char* finalURL = NULL;
        rval = curl_easy_getinfo( _handle, CURLINFO_EFFECTIVE_URL, &finalURL );

        if( rval == 0 ) {
            if( strstr( finalURL, "http" ) == finalURL ) {
                long httpCode = 0;
                rval = curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &httpCode );

                // A 206 status code indicates resume is supported. Return true!
                if( httpCode == 206 ) {
                    return 1;
                }
            }
        }
    }

    // If we've gotten this far the server does not support resume. Clear the 
    // HTTP "Range" header and return false.
    curl_easy_setopt( _handle, CURLOPT_RANGE, NULL );
    return 0;    
}

/*
    Initialize the stats ClassAd
*/
void 
CurlPlugin::init_stats( char* request_url ) {

    char* url = strdup( request_url );
    char* url_token;

    // Set the transfer protocol. If it's not http, ftp and file, then just
    // leave it blank because this transfer will fail quickly.
    if ( !strncasecmp( url, "http://", 7 ) ) {
        _file_transfer_stats->TransferProtocol = "http";
    }
    else if ( !strncasecmp( url, "https://", 8 ) ) {
        _file_transfer_stats->TransferProtocol = "https";
    }
    else if ( !strncasecmp( url, "ftp://", 6 ) ) {
        _file_transfer_stats->TransferProtocol = "ftp";
    }
    else if ( !strncasecmp( url, "file://", 7 ) ) {
        _file_transfer_stats->TransferProtocol = "file";
    }

    // Set the request host name by parsing it out of the URL
    _file_transfer_stats->TransferUrl = url;
    url_token = strtok( url, ":/" );
    url_token = strtok( NULL, "/" );
    _file_transfer_stats->TransferHostName = url_token;

    // Set the host name of the local machine using getaddrinfo().
    struct addrinfo hints, *info;
    int addrinfo_result;

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    // Look up the host name. If this fails for any reason, do not include
    // it with the stats.
    if ( ( addrinfo_result = getaddrinfo( hostname, "http", &hints, &info ) ) == 0 ) {
        _file_transfer_stats->TransferLocalMachineName = info->ai_canonname;
    }

    // Cleanup and exit
    freeaddrinfo( info );
}

/*
    Callback function provided by libcurl, which is called upon receiving
    HTTP headers. We use this to gather statistics.
*/
size_t 
CurlPlugin::header_callback( char* buffer, size_t size, size_t nitems ) {
    const char* delimiters = " \r\n";
    size_t numBytes = nitems * size;

    // Parse this HTTP header
    // We should probably add more error checking to this parse method...
    char* token = strtok( buffer, delimiters );
    while( token ) {
        // X-Cache header provides details about cache hits
        if( strcmp ( token, "X-Cache:" ) == 0 ) {
            token = strtok( NULL, delimiters );
            _global_ft_stats->HttpCacheHitOrMiss = token;
        }
        // Via header provides details about cache host
        else if( strcmp ( token, "Via:" ) == 0 ) {
            // The next token is a version number. We can ignore it.
            token = strtok( NULL, delimiters );
            // Next comes the actual cache host
            if( token != NULL ) {
                token = strtok( NULL, delimiters );
                _global_ft_stats->HttpCacheHost = token;
            }
        }
        token = strtok( NULL, delimiters );
    }
    return numBytes;
}

/*
    Write callback function for FTP file transfers.
*/
size_t 
CurlPlugin::ftp_write_callback( void* buffer, size_t size, size_t nmemb, void* stream ) {
    printf("[ftp_write_callback] called!\n");
    FILE* outfile = ( FILE* ) stream;
    return fwrite( buffer, size, nmemb, outfile); 
}

int 
main( int argc, char **argv ) {
 
    int diagnostic = 0;
    int rval = 0;
    string input_filename;
    string transfer_files;

     // Make sure there is only one command-line argument, and it's either
    // -classad or the name of an input file
    if ( argc == 2 ) {
        if ( strcmp( argv[1], "-classad" ) == 0 ) {
            printf( "%s",
                "MultipleFileSupport = true\n"
                "PluginVersion = \"0.2\"\n"
                "PluginType = \"FileTransfer\"\n"
                "SupportedMethods = \"http,https,ftp,file\"\n"
            );
            return 0;
        }
        else {
            input_filename = argv[1];
        }
    }
    if ( ( argc > 3 ) && ! strcmp( argv[3],"-diagnostic" ) ) {
        diagnostic = 1;
    } 

    // Instantiate a CurlPlugin object and handle the request
    CurlPlugin curl_plugin( diagnostic );
    rval = curl_plugin.DownloadMultipleFiles( input_filename );

    // Now that we've finished all transfers, write all the statistics we
    // gathered to stdout, with an individual classad to represent each file
    // transferred. They'll be read and processed by the FileTransfer object.
    // MRC: Implement this!
    printf("%s", curl_plugin.GetStats().c_str());

    return rval;    // 0 on success
}


