#pragma once

#include "Types.hpp"

#include <deque>




namespace Components::Menu
{
  namespace BackdropData
  {
    using Game::XModel;
    using Game::XSurface;
    using Game::XModelSurfs;
    using Game::Material;
    using Game::XRigidVertList;
    struct surface_set;

    // Masked models.
    //
    // The engine draws a model whole: every surface of the level of detail
    // it picks. To show some of a model's surfaces only, as a character put
    // together piece by piece does (see wardrobe/piece.hxx), we hand it a
    // model of its own that has only those. It is a copy of the model's
    // header with a single level of detail, the most detailed, its
    // surfaces and their materials cut down to those kept.
    //
    // The surfaces are copied too, but what they point to (their vertices,
    // triangles, and skinning) is the other model's, and so is everything
    // else the header points to (its skeleton and bounds). So a masked model
    // is good for as long as the model it was made from is loaded, and no
    // longer.
    //
    // A masked model may also show surfaces of other models, grafted onto
    // its skeleton: a torso of one body over the legs of another, say. We
    // could hand the engine both bodies instead, but it adds up the bones
    // of every model of an object, and an object may have no more than 192
    // of them, which two bodies and a head are already past. The models a
    // character is put together from share most of their skeleton, so a
    // grafted surface is skinned to the bones of the same name. A model's
    // own bones, for its gear, say, are taken to be their nearest ancestor
    // the skeleton has, which is exact for as long as no animation moves
    // them, as the idles don't. That takes copies
    // of its skinning, its vertices' bones and its rigid lists, which the
    // masked model owns; the vertices and triangles are still the other
    // model's, which has to stay loaded as well.
    //
    class masked_model
    {
    public:
      // Return the model to hand to the engine.
      //
      XModel&
      model () noexcept {return model_;}

      const XModel&
      model () const noexcept {return model_;}

      // The model points into the object, which therefore stays put.
      //
      masked_model (const masked_model&) = delete;
      masked_model& operator= (const masked_model&) = delete;

    private:
      masked_model () = default;

      friend unique_ptr<masked_model>
      mask (const XModel&, span<const surface_set>);

      string                             name_;
      vector<XSurface>                   surfaces_;
      vector<Material*>                  materials_;
      std::deque<vector<uint16_t>>       blends_;
      std::deque<vector<XRigidVertList>> rigid_;
      XModelSurfs                        model_surfaces_;
      XModel                             model_;
    };

    // Some of the surfaces of a model's most detailed level of detail, by
    // index.
    //
    struct surface_set
    {
      const XModel*        model;
      span<const uint16_t> surfaces;
    };

    // Return a model on the skeleton's bones showing the surfaces of the
    // sets: those of the skeleton's own model first, in its order, then
    // those of each other model, grafted, in the order of the sets and of
    // the model's surfaces, each surface once. Its name is the skeleton's
    // with the indices kept, prefixed with their model's name when grafted
    // (body+0+2+vest:1), which is only for the engine's diagnostics.
    //
    // The indices are those of their model's surfaces, of which there is at
    // least one among the sets.
    //
    // Throw std::invalid_argument if a grafted surface is skinned to a bone
    // that the skeleton has neither of nor of any of its ancestors (by
    // name).
    //
    unique_ptr<masked_model>
    mask (const XModel& skeleton, span<const surface_set>);

    // Return a model showing the surfaces of the model with the indices
    // only (see above).
    //
    unique_ptr<masked_model>
    mask (const XModel&, span<const uint16_t> surfaces);
  }
}
