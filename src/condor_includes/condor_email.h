/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _CONDOR_EMAIL_H
#define _CONDOR_EMAIL_H

#if defined(__cplusplus)
extern "C" {
#endif

FILE * email_open( const char *email_addr, const char *subject );

FILE * email_admin_open(const char *subject);

FILE * email_developers_open(const char *subject);

void email_close(FILE *mailer);

void email_asciifile_tail( FILE* mailer, const char* filename,
						   int lines );  

void email_corefile_tail( FILE* mailer, const char* subsystem_name );

#if defined(__cplusplus)
}
#endif

/* I, Alain Roy, moved the declaration for this down from where it was
 * above.  It's a bad move to include c++ header files within an
 * extern C.  */
#if defined(__cplusplus)
#include "condor_classad.h"

FILE * email_user_open( ClassAd* jobAd, const char *subject );

class Email
{
public:
	Email();
	~Email();

		/** If you want to write your own text, you can open a new
			message and get back the FILE*
		*/
	FILE* open( ClassAd* ad, int exit_reason = -1,
				const char* subject = NULL );
	
		/** Write exit info about the job into an open Email.
			@param ad Job to extract info from
			@param exit_reason The Condor exit_reason (not status int)
		*/
	bool writeExit( ClassAd* ad, int exit_reason );

		/** This method sucks.  As soon as we have a real solution for
			storing all 4 of these values in the job classad, it
			should be removed.  In the mean time, it's a way to write
			out the network traffic stats for the job into the email.
		*/
	void writeBytes( float run_sent, float run_recv, float tot_sent,
					 float tot_recv );

		/// Write out the introductory identification for a job
	bool writeJobId( ClassAd* ad );

		/// Send a currently open Email
	bool send();

		/// These methods handle open, write, and send, but offer no
		/// flexibility in the text of the message.
	void sendExit( ClassAd* ad, int exit_reason );
		/** This method sucks.  As soon as we have a real solution for
			storing all 4 of these values in the job classad, it
			should be removed.  In the mean time, it's a way to write
			out the network traffic stats for the job into the email.
		*/
	void sendExitWithBytes( ClassAd* ad, int exit_reason,
							float run_sent, float run_recv,
							float tot_sent, float tot_recv );
	void sendError( ClassAd* ad, const char* err_summary, 
					const char* err_msg );
	void sendHold( ClassAd* ad, const char* reason );
	void sendRemove( ClassAd* ad, const char* reason );

private:
		// // // // // //
		// Data
		// // // // // //

	FILE* fp;	/// The currently open message (if any)
	int cluster;
	int proc;


		// // // // // //
		// Methods
		// // // // // //

		/// Initialize private data
	void init();

		/** Since the email for most of our events should be so
			similar, we put the code in a shared method to avoid
			duplication.
			@param ad ClassAd for the job
			@param reason The reason we're taking the action
			@param action String describing the action we're taking
		*/
	void sendAction( ClassAd* ad, const char* reason,
					 const char* action );

	bool shouldSend( ClassAd* ad, int exit_reason = -1,
					 bool is_error = false );
};


#endif /* defined(__cplusplus) */

#endif /* _CONDOR_EMAIL_H */
