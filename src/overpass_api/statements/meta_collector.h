/** Copyright 2008, 2009, 2010, 2011, 2012 Roland Olbricht
*
* This file is part of Overpass_API.
*
* Overpass_API is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Overpass_API is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Overpass_API.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DE__OSM3S___OVERPASS_API__STATEMENTS__META_COLLECTOR_H
#define DE__OSM3S___OVERPASS_API__STATEMENTS__META_COLLECTOR_H

#include <map>
#include <set>
#include <vector>

#include "../../template_db/block_backend.h"
#include "../../template_db/random_file.h"
#include "../core/settings.h"
#include "statement.h"

using namespace std;

template< class TIndex, class TObject >
struct Meta_Collector
{
  public:
    Meta_Collector(const map< TIndex, vector< TObject > >& items,
        Transaction& transaction, const File_Properties* meta_file_prop = 0,
	bool user_data = true);
    Meta_Collector(const set< pair< TIndex, TIndex > >& used_ranges,
        Transaction& transaction, const File_Properties* meta_file_prop = 0);
    
    void reset();
    const OSM_Element_Metadata_Skeleton* get(const TIndex& index, uint32 ref);    
    const map< uint32, string >& users() const { return users_; }
    
    ~Meta_Collector()
    {
      if (meta_db)
      {
	delete db_it;
	delete meta_db;
      }
    }
  
  private:
    set< TIndex > used_indices;
    set< pair< TIndex, TIndex > > used_ranges;
    Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >* meta_db;
    typename Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >::Discrete_Iterator*
      db_it;
    typename Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >::Range_Iterator*
      range_it;
    TIndex* current_index;
    set< OSM_Element_Metadata_Skeleton > current_objects;
    map< uint32, string > users_;
};

/** Implementation --------------------------------------------------------- */

template< class TIndex, class TObject >
void generate_index_query
  (set< TIndex >& indices,
   const map< TIndex, vector< TObject > >& items)
{
  for (typename map< TIndex, vector< TObject > >::const_iterator
      it(items.begin()); it != items.end(); ++it)
    indices.insert(it->first);
}

template< class TIndex, class TObject >
Meta_Collector< TIndex, TObject >::Meta_Collector
    (const map< TIndex, vector< TObject > >& items,
     Transaction& transaction, const File_Properties* meta_file_prop, bool user_data)
  : meta_db(0), db_it(0), range_it(0), current_index(0)
{
  if (!meta_file_prop)
    return;
  
  generate_index_query(used_indices, items);
  meta_db = new Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >
      (transaction.data_index(meta_file_prop));
	  
  reset();
  
  if (user_data)
  {
    Block_Backend< Uint32_Index, User_Data > user_db
        (transaction.data_index(meta_settings().USER_DATA));
    for (Block_Backend< Uint32_Index, User_Data >::Flat_Iterator it = user_db.flat_begin();
        !(it == user_db.flat_end()); ++it)
      users_[it.object().id] = it.object().name;
  }
}
    
template< class TIndex, class TObject >
Meta_Collector< TIndex, TObject >::Meta_Collector
    (const set< pair< TIndex, TIndex > >& used_ranges_,
     Transaction& transaction, const File_Properties* meta_file_prop)
  : used_ranges(used_ranges_), meta_db(0), db_it(0), range_it(0), current_index(0)
{
  if (!meta_file_prop)
    return;
  
  meta_db = new Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >
      (transaction.data_index(meta_file_prop));
      
  reset();
}

template< class TIndex, class TObject >
void Meta_Collector< TIndex, TObject >::reset()
{
  if (!meta_db)
    return;
      
  if (db_it)
    delete db_it;
  if (range_it)
    delete range_it;
  if (current_index)
  {
    delete current_index;
    current_index = 0;
  }
  
  if (used_ranges.empty())
  {
    db_it = new typename Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >
        ::Discrete_Iterator(meta_db->discrete_begin(used_indices.begin(), used_indices.end()));
	
    if (!(*db_it == meta_db->discrete_end()))
      current_index = new TIndex(db_it->index());
    while (!(*db_it == meta_db->discrete_end()) && (*current_index == db_it->index()))
    {
      current_objects.insert(db_it->object());
      ++(*db_it);
    }
  }
  else
  {
    range_it = new typename Block_Backend< TIndex, OSM_Element_Metadata_Skeleton >
        ::Range_Iterator(meta_db->range_begin(
	    Default_Range_Iterator< TIndex >(used_ranges.begin()),
	    Default_Range_Iterator< TIndex >(used_ranges.end())));
	
    if (!(*range_it == meta_db->range_end()))
      current_index = new TIndex(range_it->index());
    while (!(*range_it == meta_db->range_end()) && (*current_index == range_it->index()))
    {
      current_objects.insert(range_it->object());
      ++(*range_it);
    }
  }
}

template< class TIndex, class TObject >
const OSM_Element_Metadata_Skeleton* Meta_Collector< TIndex, TObject >::get
    (const TIndex& index, uint32 ref)
{
  if (!meta_db)
    return 0;
  
  if ((current_index) && (*current_index < index))
  {
    current_objects.clear();
    
    if (db_it)
    {
      while (!(*db_it == meta_db->discrete_end()) && (db_it->index() < index))
	++(*db_it);
      if (!(*db_it == meta_db->discrete_end()))
	*current_index = db_it->index();
      while (!(*db_it == meta_db->discrete_end()) && (*current_index == db_it->index()))
      {
	current_objects.insert(db_it->object());
	++(*db_it);
      }
    }
    else if (range_it)
    {
      while (!(*range_it == meta_db->range_end()) && (range_it->index() < index))
	++(*range_it);
      if (!(*range_it == meta_db->range_end()))
	*current_index = range_it->index();
      while (!(*range_it == meta_db->range_end()) && (*current_index == range_it->index()))
      {
	current_objects.insert(range_it->object());
	++(*range_it);
      }
    }
  }
  
  set< OSM_Element_Metadata_Skeleton >::iterator it
      = current_objects.find(OSM_Element_Metadata_Skeleton(ref));
  if (it != current_objects.end())
    return &*it;
  else
    return 0;
}

#endif
