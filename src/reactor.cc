#include "reactor.h"

using cyclus::Material;
using cyclus::Composition;
using cyclus::toolkit::ResBuf;
using cyclus::toolkit::MatVec;
using cyclus::KeyError;
using cyclus::ValueError;
using cyclus::Request;

namespace cycamore {

Reactor::Reactor(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      n_assem_batch(0),
      assem_size(0),
      n_assem_core(0),
      n_assem_spent(0),
      n_assem_fresh(0),
      cycle_time(0),
      refuel_time(0),
      cycle_step(0),
      power_cap(0),
      power_name("power"),
      discharged(false) {
  cyclus::Warn<cyclus::EXPERIMENTAL_WARNING>(
      "the Reactor archetype "
      "is experimental");
}

#pragma cyclus def clone cycamore::Reactor

#pragma cyclus def schema cycamore::Reactor

#pragma cyclus def annotations cycamore::Reactor

#pragma cyclus def infiletodb cycamore::Reactor

#pragma cyclus def snapshot cycamore::Reactor

#pragma cyclus def snapshotinv cycamore::Reactor

#pragma cyclus def initinv cycamore::Reactor

void Reactor::InitFrom(Reactor* m) {
  #pragma cyclus impl initfromcopy cycamore::Reactor
  cyclus::toolkit::CommodityProducer::Copy(m);
}

void Reactor::InitFrom(cyclus::QueryableBackend* b) {
  #pragma cyclus impl initfromdb cycamore::Reactor

  namespace tk = cyclus::toolkit;
  tk::CommodityProducer::Add(tk::Commodity(power_name),
                             tk::CommodInfo(power_cap, power_cap));
}

void Reactor::EnterNotify() {
  cyclus::Facility::EnterNotify();
  
  // If the user ommitted fuel_prefs, we set it to zeros for each fuel
  // type.  Without this segfaults could occur - yuck.
  if (fuel_prefs.size() == 0) {
    for (int i = 0; i < fuel_outcommods.size(); i++) {
      fuel_prefs.push_back(0);
    }
  }

  // input consistency checking:
  int n = recipe_change_times.size();
  std::stringstream ss;
  if (recipe_change_commods.size() != n) {
    ss << "prototype '" << prototype() << "' has "
       << recipe_change_commods.size()
       << " recipe_change_commods vals, expected " << n << "\n";
  }
  if (recipe_change_in.size() != n) {
    ss << "prototype '" << prototype() << "' has " << recipe_change_in.size()
       << " recipe_change_in vals, expected " << n << "\n";
  }
  if (recipe_change_out.size() != n) {
    ss << "prototype '" << prototype() << "' has " << recipe_change_out.size()
       << " recipe_change_out vals, expected " << n << "\n";
  }

  n = pref_change_times.size();
  if (pref_change_commods.size() != n) {
    ss << "prototype '" << prototype() << "' has " << pref_change_commods.size()
       << " pref_change_commods vals, expected " << n << "\n";
  }
  if (pref_change_values.size() != n) {
    ss << "prototype '" << prototype() << "' has " << pref_change_values.size()
       << " pref_change_values vals, expected " << n << "\n";
  }

  if (ss.str().size() > 0) {
    throw cyclus::ValueError(ss.str());
  }
}

void Reactor::Tick() {
  // The following code must go in the Tick so they fire on the time step
  // following the cycle_step update - allowing for the all reactor events to
  // occur and be recorded on the "beginning" of a time step.  Another reason
  // they
  // can't go at the beginnin of the Tock is so that resource exchange has a
  // chance to occur after the discharge on this same time step.
  if (cycle_step == cycle_time) {
    Transmute();
    Record("CYCLE_END", "");
  }
  if (cycle_step >= cycle_time && !discharged) {
    discharged = Discharge();
  }
  if (cycle_step >= cycle_time) {
    Load();
  }

  int t = context()->time();

  // update preferences
  for (int i = 0; i < pref_change_times.size(); i++) {
    int change_t = pref_change_times[i];
    if (t != change_t) {
      continue;
    }

    std::string incommod = pref_change_commods[i];
    for (int j = 0; j < fuel_incommods.size(); j++) {
      if (fuel_incommods[j] == incommod) {
        fuel_prefs[j] = pref_change_values[i];
        break;
      }
    }
  }

  // update recipes
  for (int i = 0; i < recipe_change_times.size(); i++) {
    int change_t = recipe_change_times[i];
    if (t != change_t) {
      continue;
    }

    std::string incommod = recipe_change_commods[i];
    for (int j = 0; j < fuel_incommods.size(); j++) {
      if (fuel_incommods[j] == incommod) {
        fuel_inrecipes[j] = recipe_change_in[i];
        fuel_outrecipes[j] = recipe_change_out[i];
        break;
      }
    }
  }
}

std::set<cyclus::RequestPortfolio<Material>::Ptr> Reactor::GetMatlRequests() {
  using cyclus::RequestPortfolio;

  std::set<RequestPortfolio<Material>::Ptr> ports;
  Material::Ptr m;

  int n_assem_order =
      n_assem_core - core.count() + n_assem_fresh - fresh.count();
  if (n_assem_order == 0) {
    return ports;
  }

  for (int i = 0; i < n_assem_order; i++) {
    RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
    std::vector<Request<Material>*> mreqs;
    for (int j = 0; j < fuel_incommods.size(); j++) {
      std::string commod = fuel_incommods[j];
      double pref = fuel_prefs[j];
      Composition::Ptr recipe = context()->GetRecipe(fuel_inrecipes[j]);
      m = Material::CreateUntracked(assem_size, recipe);
      Request<Material>* r = port->AddRequest(m, this, commod, pref, true);
      mreqs.push_back(r);
    }
    port->AddMutualReqs(mreqs);
    ports.insert(port);
  }

  return ports;
}

void Reactor::GetMatlTrades(
    const std::vector<cyclus::Trade<Material> >& trades,
    std::vector<std::pair<cyclus::Trade<Material>, Material::Ptr> >&
        responses) {
  using cyclus::Trade;

  std::map<std::string, MatVec> mats = PopSpent();
  std::vector<cyclus::Trade<cyclus::Material> >::const_iterator it;
  for (int i = 0; i < trades.size(); i++) {
    std::string commod = trades[i].request->commodity();
    Material::Ptr m = mats[commod].back();
    mats[commod].pop_back();
    responses.push_back(std::make_pair(trades[i], m));
    res_indexes.erase(m->obj_id());
  }
  PushSpent(mats);  // return leftovers back to spent buffer
}

void Reactor::AcceptMatlTrades(const std::vector<
    std::pair<cyclus::Trade<Material>, Material::Ptr> >& responses) {
  std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                        cyclus::Material::Ptr> >::const_iterator trade;

  std::stringstream ss;
  int nload = std::min((int)responses.size(), n_assem_core - core.count());
  if (nload > 0) {
    ss << nload << " assemblies";
    Record("LOAD", ss.str());
  }

  for (trade = responses.begin(); trade != responses.end(); ++trade) {
    std::string commod = trade->first.request->commodity();
    Material::Ptr m = trade->second;
    index_res(m, commod);

    if (core.count() < n_assem_core) {
      core.Push(m);
    } else {
      fresh.Push(m);
    }
  }
}

std::set<cyclus::BidPortfolio<Material>::Ptr> Reactor::GetMatlBids(
    cyclus::CommodMap<Material>::type& commod_requests) {
  using cyclus::BidPortfolio;

  std::set<BidPortfolio<Material>::Ptr> ports;

  bool gotmats = false;
  std::map<std::string, MatVec> all_mats;
  for (int i = 0; i < fuel_outcommods.size(); i++) {
    std::string commod = fuel_outcommods[i];
    std::vector<Request<Material>*>& reqs = commod_requests[commod];
    if (reqs.size() == 0) {
      continue;
    } else if (!gotmats) {
      all_mats = PeekSpent();
    }

    MatVec mats = all_mats[commod];
    if (mats.size() == 0) {
      continue;
    }

    BidPortfolio<Material>::Ptr port(new BidPortfolio<Material>());

    for (int j = 0; j < reqs.size(); j++) {
      Request<Material>* req = reqs[j];
      double tot_bid = 0;
      for (int k = 0; k < mats.size(); k++) {
        Material::Ptr m = mats[k];
        tot_bid += m->quantity();
        port->AddBid(req, m, this, true);
        if (tot_bid >= req->target()->quantity()) {
          break;
        }
      }
    }

    double tot_qty = 0;
    for (int j = 0; j < mats.size(); j++) {
      tot_qty += mats[j]->quantity();
    }
    cyclus::CapacityConstraint<Material> cc(tot_qty);
    port->AddConstraint(cc);
    ports.insert(port);
  }

  return ports;
}

void Reactor::Tock() {
  if (cycle_step >= cycle_time + refuel_time && core.count() == n_assem_core) {
    discharged = false;
    cycle_step = 0;
  }

  if (cycle_step == 0 && core.count() == n_assem_core) {
    Record("CYCLE_START", "");
  }

  if (cycle_step >= 0 && cycle_step < cycle_time &&
      core.count() == n_assem_core) {
    cyclus::toolkit::RecordTimeSeries<cyclus::toolkit::POWER>(this, power_cap);
  } else {
    cyclus::toolkit::RecordTimeSeries<cyclus::toolkit::POWER>(this, 0);
  }

  // "if" prevents starting cycle after initial deployment until core is full
  // even though cycle_step is its initial zero.
  if (cycle_step > 0 || core.count() == n_assem_core) {
    cycle_step++;
  }
}

void Reactor::Transmute() {
  // safe to assume full core.
  MatVec old = core.PopN(n_assem_batch);
  MatVec tail = core.PopN(core.count());
  core.Push(old);
  core.Push(tail);

  std::stringstream ss;
  ss << old.size() << " assemblies";
  Record("TRANSMUTE", ss.str());

  for (int i = 0; i < old.size(); i++) {
    old[i]->Transmute(context()->GetRecipe(fuel_outrecipe(old[i])));
  }
}

std::map<std::string, MatVec> Reactor::PeekSpent() {
  std::map<std::string, MatVec> mapped;
  MatVec mats = spent.PopN(spent.count());
  spent.Push(mats);
  for (int i = 0; i < mats.size(); i++) {
    std::string commod = fuel_outcommod(mats[i]);
    mapped[commod].push_back(mats[i]);
  }
  return mapped;
}

bool Reactor::Discharge() {
  if (n_assem_spent - spent.count() < n_assem_batch) {
    Record("DISCHARGE", "failed");
    return false;  // not enough room in spent buffer
  }

  int npop = std::min(n_assem_batch, core.count());

  std::stringstream ss;
  ss << npop << " assemblies";
  Record("DISCHARGE", ss.str());

  spent.Push(core.PopN(npop));
  return true;
}

void Reactor::Load() {
  int n = std::min(n_assem_core - core.count(), fresh.count());
  if (n == 0) {
    return;
  }

  std::stringstream ss;
  ss << n << " assemblies";
  Record("LOAD", ss.str());
  core.Push(fresh.PopN(n));
}

std::string Reactor::fuel_incommod(Material::Ptr m) {
  int i = res_indexes[m->obj_id()];
  if (i >= fuel_incommods.size()) {
    throw KeyError("cycamore::Reactor - no incommod for material object");
  }
  return fuel_incommods[i];
}

std::string Reactor::fuel_outcommod(Material::Ptr m) {
  int i = res_indexes[m->obj_id()];
  if (i >= fuel_outcommods.size()) {
    throw KeyError("cycamore::Reactor - no outcommod for material object");
  }
  return fuel_outcommods[i];
}

std::string Reactor::fuel_inrecipe(Material::Ptr m) {
  int i = res_indexes[m->obj_id()];
  if (i >= fuel_inrecipes.size()) {
    throw KeyError("cycamore::Reactor - no inrecipe for material object");
  }
  return fuel_inrecipes[i];
}

std::string Reactor::fuel_outrecipe(Material::Ptr m) {
  int i = res_indexes[m->obj_id()];
  if (i >= fuel_outrecipes.size()) {
    throw KeyError("cycamore::Reactor - no outrecipe for material object");
  }
  return fuel_outrecipes[i];
}

double Reactor::fuel_pref(Material::Ptr m) {
  int i = res_indexes[m->obj_id()];
  if (i >= fuel_prefs.size()) {
    return 0;
  }
  return fuel_prefs[i];
}

void Reactor::index_res(cyclus::Resource::Ptr m, std::string incommod) {
  for (int i = 0; i < fuel_incommods.size(); i++) {
    if (fuel_incommods[i] == incommod) {
      res_indexes[m->obj_id()] = i;
      return;
    }
  }
  throw ValueError(
      "cycamore::Reactor - received unsupported incommod material");
}

std::map<std::string, MatVec> Reactor::PopSpent() {
  MatVec mats = spent.PopN(spent.count());
  std::map<std::string, MatVec> mapped;
  for (int i = 0; i < mats.size(); i++) {
    std::string commod = fuel_outcommod(mats[i]);
    mapped[commod].push_back(mats[i]);
  }

  // needed so we trade away oldest assemblies first
  std::map<std::string, MatVec>::iterator it;
  for (it = mapped.begin(); it != mapped.end(); ++it) {
    std::reverse(it->second.begin(), it->second.end());
  }

  return mapped;
}

void Reactor::PushSpent(std::map<std::string, MatVec> leftover) {
  std::map<std::string, MatVec>::iterator it;
  for (it = leftover.begin(); it != leftover.end(); ++it) {
    // undo reverse in PopSpent to make sure oldest assemblies come out first
    std::reverse(it->second.begin(), it->second.end());
    spent.Push(it->second);
  }
}

void Reactor::Record(std::string name, std::string val) {
  context()
      ->NewDatum("ReactorEvents")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Event", name)
      ->AddVal("Value", val)
      ->Record();
}

extern "C" cyclus::Agent* ConstructReactor(cyclus::Context* ctx) {
  return new Reactor(ctx);
}

}  // namespace cycamore
