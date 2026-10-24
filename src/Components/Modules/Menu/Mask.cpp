#include "STDInclude.hpp"

#include "Mask.hpp"

#include <format>
#include <stdexcept> // invalid_argument
#include <algorithm> // ranges::{all_of,any_of,copy,find,find_if,sort,unique}()


using namespace std;
using Game::XModelLodInfo;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      // The size of a bone's skinning matrix, which is what a surface's
      // skinning gives its bones as offsets of.
      //
      constexpr uint16_t bone_matrix_size (64);

      // The bones that part bits have room for.
      //
      constexpr size_t max_bones (6 * 32);

      // The bit of the bone in part bits, which run from the top of each
      // word, as the engine's do.
      //
      constexpr unsigned int
      part_bit (size_t b) noexcept
      {
        return 0x80000000u >> (b & 31);
      }

      // How a grafted surface's bones are found on the skeleton.
      //
      class bone_map
      {
      public:
        // Map the bones of the model onto those of the skeleton by name, a
        // bone the skeleton doesn't have onto its nearest ancestor that it
        // does, for the surface of the index, which diagnostics name.
        //
        // Note that a bone's skinning matrix is its pose times the inverse
        // of its bind pose, so for a bone that only follows its parent the
        // two are the same, which is what makes taking one for the other
        // exact.
        //
        bone_map (const XModel& skeleton, const XModel& m, uint16_t surface)
          : skeleton_ (skeleton), model_ (m), surface_ (surface)
        {
          auto find = [&skeleton] (uint16_t n) -> optional<size_t>
          {
            for (size_t j (0); j != skeleton.numBones; ++j)
            {
              if (skeleton.boneNames[j] == n)
                return j;
            }

            return nullopt;
          };

          const size_t roots (static_cast<unsigned char> (m.numRootBones));

          for (size_t i (0); i != m.numBones; ++i)
          {
            optional<size_t> r (find (m.boneNames[i]));

            // Walk up the model's own hierarchy. Note that the parents come
            // before their children, so what we map onto is already known.
            //
            if (!r && i >= roots && m.parentList != nullptr)
            {
              const size_t p (
                i - static_cast<unsigned char> (m.parentList[i - roots]));

              if (p < i)
                r = bones_[p];
            }

            bones_.push_back (r);
          }
        }

        // Return the skeleton's bone for the model's.
        //
        // Throw invalid_argument if the skeleton has no such bone.
        //
        size_t
        operator() (size_t b) const
        {
          if (b >= bones_.size () || !bones_[b])
            throw invalid_argument (
              format ("surface {} of {} is skinned to bone {}, of which {} has "
                      "neither the bone nor an ancestor",
                      surface_,
                      model_.name != nullptr ? model_.name : "",
                      b,
                      skeleton_.name != nullptr ? skeleton_.name : ""));

          return *bones_[b];
        }

        // As above but for a bone given as its skinning matrix's offset.
        //
        uint16_t
        offset (uint16_t o) const
        {
          return static_cast<uint16_t> (
            (*this) (o / bone_matrix_size) * bone_matrix_size);
        }

      private:
        const XModel&            skeleton_;
        const XModel&            model_;
        uint16_t                 surface_;
        vector<optional<size_t>> bones_;
      };
    }

    unique_ptr<masked_model>
    mask (const XModel& skeleton, span<const surface_set> sets)
    {
      auto valid = [] (const surface_set& s)
      {
        const XModelLodInfo& l (s.model->lodInfo[0]);

        return s.model->numLods > 0 &&
               l.surfs != nullptr &&
               ranges::all_of (s.surfaces, [&l] (uint16_t i)
               {
                 return i < l.numsurfs;
               });
      };

      assert (skeleton.numLods > 0 &&
                   skeleton.lodInfo[0].surfs != nullptr);
      assert (ranges::all_of (sets, valid));
      assert (ranges::any_of (sets, [] (const surface_set& s)
      {
        return !s.surfaces.empty ();
      }));

      // Gather the surfaces by model, the skeleton's own first, then the
      // others in the order they first come in, each surface once, in its
      // model's order.
      //
      vector<pair<const XModel*, vector<uint16_t>>> gs {{&skeleton, {}}};

      for (const surface_set& s: sets)
      {
        auto i (ranges::find (gs, s.model, &pair<const XModel*,
                                                 vector<uint16_t>>::first));

        if (i == gs.end ())
          i = gs.insert (gs.end (), {s.model, {}});

        i->second.insert (i->second.end (),
                          s.surfaces.begin (),
                          s.surfaces.end ());
      }

      unique_ptr<masked_model> r (new masked_model);

      r->name_ = skeleton.name != nullptr ? skeleton.name : "";

      for (auto& [m, is]: gs)
      {
        ranges::sort (is);
        is.erase (ranges::unique (is).begin (), is.end ());

        const XModelLodInfo& l (m->lodInfo[0]);
        const bool grafted (m != &skeleton);

        for (uint16_t i: is)
        {
          r->name_ += grafted
            ? format ("+{}:{}", m->name != nullptr ? m->name : "", i)
            : format ("+{}", i);

          XSurface s (l.surfs[i]);

          // Skin a grafted surface to the skeleton's bones: the bones of
          // its vertices, its rigid lists, and its part bits. The weights
          // stay as they are.
          //
          if (grafted)
          {
            const bone_map map (skeleton, *m, i);

            if (s.vertInfo.vertsBlend != nullptr)
            {
              const short* c (s.vertInfo.vertCount);

              vector<uint16_t>& bs (r->blends_.emplace_back (
                s.vertInfo.vertsBlend,
                s.vertInfo.vertsBlend + c[0] * 1 + c[1] * 3 + c[2] * 5 +
                                        c[3] * 7));

              // A vertex of k + 1 weights has its first bone, then each
              // further bone followed by its weight.
              //
              for (size_t k (0), p (0); k != 4; ++k)
              {
                for (short v (0); v != c[k]; ++v, p += k * 2 + 1)
                {
                  bs[p] = map.offset (bs[p]);

                  for (size_t w (0); w != k; ++w)
                    bs[p + w * 2 + 1] = map.offset (bs[p + w * 2 + 1]);
                }
              }

              s.vertInfo.vertsBlend = bs.data ();
            }

            if (s.vertList != nullptr && s.vertListCount != 0)
            {
              vector<XRigidVertList>& vs (r->rigid_.emplace_back (
                s.vertList,
                s.vertList + s.vertListCount));

              for (XRigidVertList& v: vs)
                v.boneOffset = map.offset (v.boneOffset);

              s.vertList = vs.data ();
            }

            int bits[6] {};

            for (size_t b (0); b != max_bones; ++b)
            {
              if ((static_cast<unsigned int> (s.partBits[b >> 5]) &
                   part_bit (b)) != 0)
              {
                const size_t x (map (b));
                bits[x >> 5] |= static_cast<int> (part_bit (x));
              }
            }

            ranges::copy (bits, s.partBits);
          }

          r->surfaces_.push_back (s);
          r->materials_.push_back (m->materialHandles[l.surfIndex + i]);
        }
      }

      // The parts that the surfaces are skinned to, which is what the
      // engine goes by to skip a model whose parts are all hidden.
      //
      int bits[6] {};

      for (const XSurface& s: r->surfaces_)
      {
        for (size_t j (0); j != 6; ++j)
          bits[j] |= s.partBits[j];
      }

      const unsigned short n (static_cast<unsigned short> (
                                r->surfaces_.size ()));

      XModelSurfs& ms (r->model_surfaces_);
      ms = {};
      ms.name = r->name_.c_str ();
      ms.surfs = r->surfaces_.data ();
      ms.numsurfs = n;
      ranges::copy (bits, ms.partBits);

      // Note that the static model cache's slots are the other model's, so
      // the level of detail we keep claims none: a model that is posed
      // every frame doesn't use it anyway.
      //
      XModel& x (r->model_);
      x = skeleton;
      x.name = r->name_.c_str ();
      x.numsurfs = static_cast<char> (n);
      x.materialHandles = r->materials_.data ();

      XModelLodInfo& xl (x.lodInfo[0]);
      xl.numsurfs = n;
      xl.surfIndex = 0;
      xl.modelSurfs = &ms;
      xl.surfs = r->surfaces_.data ();
      ranges::copy (bits, xl.partBits);
      xl.smcBaseIndexPlusOne = 0;
      xl.smcSubIndexMask = 0;
      xl.smcBucket = 0;

      for (size_t i (1); i != 4; ++i)
        x.lodInfo[i] = {};

      x.numLods = 1;
      x.maxLoadedLod = 0;

      return r;
    }

    unique_ptr<masked_model>
    mask (const XModel& m, span<const uint16_t> ss)
    {
      const surface_set s {&m, ss};
      return mask (m, span<const surface_set> (&s, 1));
    }
  }
}
