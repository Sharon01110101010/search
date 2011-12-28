#include "player.hpp"
#include "lvl.hpp"
#include <vector>
#include <string>
#include <cstdio>

void fatal(const char*, ...);
extern "C" unsigned long hashbytes(unsigned char[], unsigned int);

struct Plat2d {

	static const unsigned int Ops[];
	static const unsigned int Nops;

	enum { UnitCost = true };

	typedef double Cost;
	static const double InfCost = -1;

	typedef int Oper;
	static const int Nop = -1;

	Plat2d(FILE*);

	struct State {
		State(void) { }

		State(unsigned int x, unsigned int y, unsigned int z,
			unsigned int w, unsigned int h) : player(x, y, w, h) { }

		Player player;
	};

	struct PackedState {
		unsigned long hash(void) {
			static const unsigned int sz = sizeof(x) +
				sizeof(y) + sizeof(dy) + sizeof(jframes);
			unsigned char bytes[sz];
			unsigned int i = 0;
			char *p = (char*) &x;
			for (unsigned int j = 0; j < sizeof(x); j++)
				bytes[i++] = p[j];
			p = (char*) &y;
			for (unsigned int j = 0; j < sizeof(y); j++)
				bytes[i++] = p[j];
			p = (char*) &dy;
			for (unsigned int j = 0; j < sizeof(dy); j++)
				bytes[i++] = p[j];
			bytes[i++] = jframes;
			assert (i <= sz);
			return hashbytes(bytes, i);
		}

		bool eq(PackedState &o) const {
			return jframes == o.jframes &&
				doubleeq(x, o.x) &&
				doubleeq(y, o.y) &&
				doubleeq(dy, o.dy);
		}

		double x, y, dy;
		// The body's fall flag is packed as the high-order bit
		// of jframes.
		unsigned char jframes;
	};

	struct Undo {
		Undo(State&, Oper) { }
	};

	State initialstate(void);

	Cost h(State &s) {
		return heuclidean(s);
	}

	Cost d(State &s) {
		return deuclidean(s);
	}

	bool isgoal(State &s) {
		Lvl::Blkinfo bi = lvl.majorblk(s.player.body.bbox);
		return bi.x == gx && bi.y == gy;
	}

	unsigned int nops(State &s) {
 		// If jumping will have no effect then allow left, right and jump.
		// This is a bit of a hack, but the 'jump' action that is allowed
		// here will end up being a 'do nothing' and just fall action.
		// Effectively, we prune off the jump/left and jump/right actions
		// since they are the same as just doing left and right in this case.
		if (!s.player.canjump())
			return 3;
		return Nops;
	}

	Oper nthop(State &s, unsigned int n) {
		return Ops[n];
	}

	Oper revop(State &s, Oper op) {
		return Nop;
	}

	void undo(State &s, Undo &u) { }

	State &apply(State &buf, State &s, Cost &c, Oper op) {
		assert (op != Nop);
		buf = s;
		buf.player.act(lvl, (unsigned int) op);
		c = Point::distance(s.player.body.bbox.min,
			buf.player.body.bbox.min);
		return buf;
	}

	void pack(PackedState &dst, State &src) {
		dst.x = src.player.body.bbox.min.x;
		dst.y = src.player.body.bbox.min.y;
		dst.dy = src.player.body.dy;
		dst.jframes = src.player.jframes;
		if (src.player.body.fall)
			dst.jframes |= 1 << 7;
	}

	State &unpack(State &buf, PackedState &pkd) {
		buf.player.jframes = pkd.jframes & 0x7F;
		buf.player.body.fall = pkd.jframes & (1 << 7);
		buf.player.body.dy = pkd.dy;
		buf.player.body.bbox.min.x = pkd.x;
		buf.player.body.bbox.min.y = pkd.y;
		buf.player.body.bbox.max.x = pkd.x + Player::Width;
		buf.player.body.bbox.max.y = pkd.y + Player::Height;
		return buf;
	}

	static void dumpstate(FILE *out, State &s) {
		fprintf(out, "%g, %g\n", s.player.loc().x, s.player.loc().y);
	}

	unsigned int gx, gy;	// goal tile location
	Lvl lvl;

private:

	// heuclidean computes the Euclidean distance of the center
	// point of the player to the goal.  That is, if the player is level
	// with the goal tile in the y direction or is level with it in the
	// x direction and above it then the Euclidean distance to the
	// nearest side is returned.  Otherwise, the minimum of the
	// Euclidean distances from the player's center point to the four
	// corners of the goal block is returned.
	Cost heuclidean(State &s) {
		static const double W = Tile::Width;
		static const double H = Tile::Height;

		const Lvl::Blkinfo &bi = lvl.majorblk(s.player.body.bbox);
		if (bi.x == gx && bi.y == gy)
			return 0;

		const Point &loc = s.player.body.bbox.center();
		Point goal;
		if (bi.y == gy)
			goal.y = loc.y;
		else if (bi.y < gy)
			goal.y = (gy - 1) * H;
		else
			goal.y = gy * H;
		if (bi.x == gx)
			goal.x = loc.x;
		else if (bi.x < gx)
			goal.x = gx * W;
		else
			goal.x = (gx + 1) * W;

		return Point::distance(loc, goal);		
	}

	Cost deuclidean(State &s) {
		static const double W = Tile::Width / Player::runspeed();
		static const double H =
			Tile::Height / Player::jmpspeed() > Tile::Height / Body::Maxdy ?
				Tile::Height / Player::jmpspeed() :
				Tile::Height / Body::Maxdy;

		const Lvl::Blkinfo &bi = lvl.majorblk(s.player.body.bbox);
		if (bi.x == gx && bi.y == gy)
			return 0;

		const Point &loc = s.player.body.bbox.center();
		Point goal;
		if (bi.y == gy)
			goal.y = loc.y;
		else if (bi.y < gy)
			goal.y = (gy - 1) * H;
		else
			goal.y = gy * H;
		if (bi.x == gx)
			goal.x = loc.x;
		else if (bi.x < gx)
			goal.x = gx * W;
		else
			goal.x = (gx + 1) * W;

		return Point::distance(loc, goal);		
	}
};

// controlstr converts a vector of controls to an ASCII string.
std::string controlstr(const std::vector<unsigned int>&);

// controlvec converts a string of controls back into a vector.
std::vector<unsigned int> controlvec(const std::string&);
