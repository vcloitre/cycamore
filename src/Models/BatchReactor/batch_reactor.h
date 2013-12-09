// batch_reactor.h
#ifndef CYCAMORE_MODELS_BATCHREACTOR_BATCH_REACTOR_H_
#define CYCAMORE_MODELS_BATCHREACTOR_BATCH_REACTOR_H_

#include <string>
#include <queue>
#include <map>

#include "bid_portfolio.h"
#include "capacity_constraint.h"
#include "commodity_producer.h"
#include "exchange_context.h"
#include "enrichment.h"
#include "facility_model.h"
#include "resource_buff.h"
#include "request_portfolio.h"
#include "material.h"

// forward declarations
namespace cycamore {
class BatchReactor;
} // namespace cycamore
namespace cyclus {  
class Context;
} // namespace cyclus

namespace cycamore {

/// @class BatchReactor
///
/// @section introduction Introduction
/// The BatchReactor is a facility that models batch processing. It has three
/// buffers which hold batches of materials: reserves, core, and
/// storage. Incoming material orders are placed into reserves, from which the
/// core is provided batches during refueling. When a process has been
/// completed, batches are moved from the core into storage. Requests for
/// material are bid upon based on the state of the material in storage.
///
/// @section params Parameters
/// A BatchReactor has the following tuneable parameters:
///   #. batch_size : the size of batches
///   #. n_batches : the number of batches that constitute a full core
///   #. process_time : the number of timesteps a batch process takes
///   #. n_load : the number of batches processed at any given time (i.e.,
///   n_load is unloaded and reloaded after a process is finished
///   #. n_reserves : the preferred number of batches in reserve
///   #. preorder_time : the amount of time before a process is finished to
///   order fuel
///   #. refuel_time : the number of timesteps required to reload the core after
///   a process has finished
///   #. in_commodity : the name of the input commodity
///   #. in_recipe : the name of the input recipe
///   #. out_commodity : the name of the output commodity
///   #. out_recipe : the name of the output recipe
/// 
/// @section requests Requests  
/// A BatchReactor will make a request for its input commodity if the
/// preorder_time has been reached and there are less than n_reserves batches in
/// its reserves.
///
/// A special case exists when the reactor first enters the simulation, where it
/// will order as much fuel as is needed to fill its full core.
/// 
/// @section bids Bids
/// A BatchReactor will bid on any request for its out_commodity, up to its
/// storage buffer quantity.
///
/// @section ics Initial Conditions
/// A BatchReactor can be deployed with any number of batches in its reserve,
/// core, and storage buffers.
///
/// @todo add decommissioning behavior if material is still in storage
class BatchReactor : public cyclus::FacilityModel,
      public cyclus::CommodityProducer {
 public:
  /// @brief defines all possible phases this facility can be in
  enum Phase {
    INITIAL, ///< The initial phase, after the facility is built but before it is
             /// filled
    PROCESS, ///< The processing phase
    WAITING, ///< The waiting phase, while the factility is waiting for fuel
             /// between processes
  };

  /// @brief a struct for initial conditions
  struct InitCond {
    InitCond(int n_reserves, int n_core, int n_storage)
     : n_reserves(n_reserves),
       n_core(n_core),
       n_storage(n_storage) {};
    
    int n_reserves;
    int n_core;
    int n_storage;
  };
  
  /* --- Module Members --- */
  /// @param ctx the cyclus context for access to simulation-wide parameters
  BatchReactor(cyclus::Context* ctx);
  
  virtual ~BatchReactor();
  
  virtual cyclus::Model* Clone();
  
  virtual std::string schema();

  /// Initialize members related to derived module class
  /// @param qe a pointer to a cyclus::QueryEngine object containing
  /// initialization data
  virtual void InitModuleMembers(cyclus::QueryEngine* qe);

  /// Print information about this model
  virtual std::string str();
  /* --- */

  /* --- Facility Members --- */
  /// perform module-specific tasks when entering the simulation 
  virtual void Deploy(cyclus::Model* parent);
  /* --- */

  /* --- Agent Members --- */  
  /// The HandleTick function specific to the BatchReactor.
  /// @param time the time of the tick
  virtual void HandleTick(int time);
  
  /// The HandleTick function specific to the BatchReactor.
  /// @param time the time of the tock
  virtual void HandleTock(int time);
  
  /// @brief The EnrichmentFacility request Materials of its given
  /// commodity. 
  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
      GetMatlRequests();

  /// @brief The EnrichmentFacility place accepted trade Materials in their
  /// Inventory
  virtual void AcceptMatlTrades(
      const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
      cyclus::Material::Ptr> >& responses);
  
  /// @brief Responds to each request for this facility's commodity.  If a given
  /// request is more than this facility's inventory or SWU capacity, it will
  /// offer its minimum of its capacities.
  virtual std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr>
      GetMatlBids(const cyclus::CommodMap<cyclus::Material>::type&
                  commod_requests);
  
  /// @brief respond to each trade with a material enriched to the appropriate
  /// level given this facility's inventory
  ///
  /// @param trades all trades in which this trader is the supplier
  /// @param responses a container to populate with responses to each trade
  virtual void GetMatlTrades(
    const std::vector< cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
    cyclus::Material::Ptr> >& responses);
  /* --- */

  /* --- BatchReactor Members --- */
  /// @brief the processing time required for a full batch process before
  /// refueling
  inline void process_time(int t) { process_time_ = t; }
  inline int process_time() const { return process_time_; }
  
  /// @brief the time it takes to refuel
  inline void refuel_time(int t) { refuel_time_ = t; }
  inline int refuel_time() const { return refuel_time_; }
  
  /// @brief the amount of time an order should be placed for new fuel before a
  /// process is finished
  inline void preorder_time(int t) { preorder_time_ = t; }
  inline int preorder_time() const { return preorder_time_; }

  /// @brief the starting time of the last (current) process
  inline void start_time(int t) { start_time_ = t; }
  inline int start_time() const { return start_time_; }

  /// @brief the ending time of the last (current) process
  inline int end_time() const { return start_time() + process_time(); }

  /// @brief the time orders should be taking place for the next refueling
  inline int order_time() const { return end_time() - preorder_time(); }

  /// @brief the number of batches in a full reactor
  inline void n_batches(int n) { n_batches_ = n; }
  inline int n_batches() const { return n_batches_; }

  /// @brief the number of batches in reactor refuel loading/unloading
  inline void n_load(int n) { n_load_ = n; }
  inline int n_load() const { return n_load_; }

  /// @brief the preferred number of fresh fuel batches to keep in reserve
  inline void n_reserves(int n) { n_reserves_ = n; }
  inline int n_reserves() const { return n_reserves_; }

  /// @brief the number of batches currently in the reactor
  inline int n_core() const { return core_.count(); }

  /// @brief the size of a batch 
  inline void batch_size(double size) { batch_size_ = size; }
  inline double batch_size() { return batch_size_; }

  /// @brief the input commodity
  inline void in_commodity(std::string name) { in_commodity_ = name; }
  inline std::string in_commodity() const { return in_commodity_; }

  /// @brief the input recipe
  inline void in_recipe(std::string name) { in_recipe_ = name; }
  inline std::string in_recipe() const { return in_recipe_; }
  
  /// @brief the output commodity
  inline void out_commodity(std::string name) { out_commodity_ = name; }
  inline std::string out_commodity() const { return out_commodity_; }
  
  /// @brief the output recipe
  inline void out_recipe(std::string name) { out_recipe_ = name; }
  inline std::string out_recipe() const { return out_recipe_; }
  
  /// @brief the current phase
  void phase(Phase p);
  inline Phase phase() const { return phase_; }

  /// @brief this facility's initial conditions
  inline void  ics(const InitCond& ics) { ics_ = ics; }
  inline InitCond ics() const { return ics_; }

 private:
  /// @brief refuels the reactor until it is full or reserves_ is out of
  /// batches. If the core is full after refueling, the Phase is set to PROCESS.
  void Refuel_();

  /// @brief moves a batch from reserves_ to core_
  void MoveBatchIn_();
  
  /// @brief moves a batch from core_ to storage_
  void MoveBatchOut_();

  /// @brief construct a request portfolio for an order of a given size
  cyclus::RequestPortfolio<cyclus::Material>::Ptr GetOrder_(double size);

  /// @brief Add a blob of incoming material to reserves_
  ///
  /// The last material to join reserves_ is first investigated to see if it is
  /// of batch_size_. If not, material from mat is added to it and it is
  /// returned to reserves_. If more material remains, chunks of batch_size_ are
  /// removed and added to reserves_. The final chunk may be <= batch_size_.
  void AddBatches_(cyclus::Material::Ptr mat);

  /// @brief adds phase names to phase_names_ map
  void SetUpPhaseNames_();
  
  static std::map<Phase, std::string> phase_names_;
  int process_time_;
  int preorder_time_;
  int refuel_time_;
  int start_time_;
  int n_batches_;
  int n_load_;
  int n_reserves_;
  double batch_size_;
  std::string in_commodity_;
  std::string out_commodity_;
  std::string in_recipe_;
  std::string out_recipe_;
  Phase phase_;
  InitCond ics_;

  /// @brief allows only batches to enter reserves_
  cyclus::Material::Ptr spillover_;
  
  /// @brief a cyclus::ResourceBuff for material before they enter the core,
  /// with all materials guaranteed to be of batch_size_
  cyclus::ResourceBuff reserves_;
  
  /// @brief a cyclus::ResourceBuff for material while they are inside the core,
  /// with all materials guaranteed to be of batch_size_
  cyclus::ResourceBuff core_;

  /// @brief a cyclus::ResourceBuff for material once they leave the core.  
  /// @warning no guarantee can be made to the size of each item in storage_, as
  /// requests can be met that are larger or smaller than batch_size_
  cyclus::ResourceBuff storage_;

  friend class BatchReactorTest;
  /* --- */
};

} // namespace cycamore

#endif // CYCAMORE_MODELS_BATCHREACTOR_BATCH_REACTOR_H_