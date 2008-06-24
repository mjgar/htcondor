/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef __MATCH_MAKER_RESOURCES_H__
#define __MATCH_MAKER_RESOURCES_H__

#include <list>
#include <map>

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#ifndef WANT_CLASSAD_NAMESPACE
# define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

#include "match_maker_lease.h"

// Map leaseId's to the lease ClassAd -- for internal use only
struct MatchMakerLeaseEnt
{
	classad::ClassAd	*m_lease_ad;
	int					m_lease_number;
	classad::ClassAd	*m_leases_ad;
	string				m_resource_name;
	int					m_expiration;
};

// Statistics
struct MatchMakerStats
{
	int			m_num_resources;
	int			m_num_xfer_records;
	int			m_num_valid_xfers;
	int			m_num_busy_xfers;
};

class MatchMakerResources
{


  public:
	MatchMakerResources( void );
	~MatchMakerResources( void );

	int init( void );
	int setCollectionLog( const char *file );
	int shutdownFast( void );
	int shutdownGraceful( void );
	int restoreLeases( void );

	// Public query operations
	bool QuerySetAd(
		const classad::ClassAd		&req_ad
		);
	bool QueryStart(
		const classad::ClassAd		&req_ad
		);
	bool QueryStart(
		void
		);
	classad::ClassAd *QueryCurrent(
		void
		);
	bool QueryNext(
		void
		);


	// Lease operations
	int GetLeases(
		classad::ClassAd			&resource_ad,
		classad::ClassAd			&request_ad,
		int							target_count,
		list<classad::ClassAd *>	&leases
		);
	int RenewLeases(
		list<const MatchMakerLease *> &requests,
		list<MatchMakerLease *> &leases
		);
	int ReleaseLeases(
		list<const MatchMakerLease *> &requests
		);

	// Resource operations
	int AddResource(
		classad::ClassAd			*resource_ad
		);
	int PruneResources(
		void
		);
	int StartExpire(
		void
		);
	int PruneExpired(
		void
		);

	int GetStats(
		MatchMakerStats				&stats
		);

	// Set various parameters
	void setAdDebug(
		bool						enable = false
		);
	void setMaxLease(
		int							max
		);
	void setMaxLeaseTotal(
		int							max
		);
	void setDefaultMaxLease(
		int							def
		);

  private:

	// Private methods
	MatchMakerLeaseEnt *FindLease (
		const MatchMakerLease		&in_lease_ad
		);

	// Resource ad maniuplation
	bool InsertResourceAd(
		const string				&resource,
		classad::ClassAd			*ad
		);
	classad::ClassAd *GetResourceAd(
		const string				&resource
		);
	bool RemoveResourceAd(
		const string				&resource
		);
	bool UpdateResourceAd(
		const string				&resource,
		const classad::ClassAd		&udpates
		);
	bool UpdateResourceAd(
		const string				&resource,
		classad::ClassAd			*udpates
		);

	// Leases ad manipulation
	bool InsertLeasesAd(
		const string				&resource,
		classad::ClassAd			*ad
		);
	bool RemoveLeasesAd(
		const string				&resource
		);
	classad::ClassAd *GetLeasesAd(
		const string				&resource
		);
	bool UpdateLeasesAd(
		const string				&resource,
		const classad::ClassAd		&updates
		);
	bool UpdateLeasesAd(
		const string				&resource,
		classad::ClassAd			*updates
		);

	// Other lease maniuplations
	int SetLeaseStates(
		const string				&resource_name,
		int							resource_transfers,
		classad::ClassAd			&leases_ad,
		int							&lease_count
		);
	bool TerminateLease(
		MatchMakerLeaseEnt			&lease
		);
	int ExpireLeases(
		void
		);
	bool UpdateLeaseAd(
		const string				&resource,
		int							lease_number,
		const classad::ClassAd		&updates
		);
	bool UpdateLeaseAd(
		const string				&resource,
		int							lease_number,
		classad::ClassAd			*updates
		);

	int GetLeaseDuration(
		const classad::ClassAd		&resource_ad,
		const MatchMakerLease		&request
		);
	int GetLeaseDuration(
		const classad::ClassAd		&resource_ad,
		const classad::ClassAd		&request_ad
		);
	int GetLeaseDuration(
		const classad::ClassAd		&resource_ad,
		int							requsted_duration
		);


	// Internal query methods
	bool QueryStart(
		classad::LocalCollectionQuery	&query,
		classad::ViewName				&view,
		const string					&expr_str
		);
	classad::ClassAd *QueryCurrent(
		classad::LocalCollectionQuery	&query,
		string							&key
		);
	bool QueryNext(
		classad::LocalCollectionQuery	&query,
		string							&key
		);

	string							m_view_key;
	classad::ClassAdCollection		m_collection;
	classad::ViewName				m_root_view;
	classad::ViewName				m_resources_view;
	classad::ViewName				m_leases_view;
	const char						*m_collection_log;

	// Query
	classad::LocalCollectionQuery	m_search_query;
	classad::ClassAd				m_search_query_ad;
	string							m_search_query_key;

	// Collection just for expiring leases
	classad::ClassAdCollection		m_lease_ad_collection;
	classad::LocalCollectionQuery	m_expiration_query;

    map<string, MatchMakerLeaseEnt*, less<string> > m_used_leases;
	int								m_default_max_lease_duration;
	int								m_max_lease_duration;
	int								m_max_lease_total;
	int								m_lease_id_number;

	// Match statistics
	MatchMakerStats					m_stats;
	bool							m_enable_ad_debug;
};


#endif	//__MATCH_MAKER_RESOURCES_H__
