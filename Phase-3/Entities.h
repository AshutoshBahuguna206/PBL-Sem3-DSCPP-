#pragma once
#include <string>
#include <utility>
#include <vector>

using Id = std::string;

// Abstract base: every game object has a unique ID, a name and a description.
class Entity {
public:
    Entity(Id id, std::string name, std::string description)
        : id_(std::move(id)), name_(std::move(name)), description_(std::move(description)) {}
    virtual ~Entity() = default;
    virtual std::string kind() const = 0;  // polymorphism: UI/DB can treat all entities uniformly

    const Id& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }

private:
    Id id_;
    std::string name_, description_;
};

// A node in the world graph. exits() are its outgoing edges.
// requiredClue: if non-empty, the player must hold that clue to enter.
class Location : public Entity {
public:
    Location(Id id, std::string name, std::string desc, std::vector<Id> exits, Id requiredClue = "")
        : Entity(std::move(id), std::move(name), std::move(desc)),
          exits_(std::move(exits)), requiredClue_(std::move(requiredClue)) {}
    std::string kind() const override { return "Location"; }
    const std::vector<Id>& exits() const { return exits_; }
    const Id& requiredClue() const { return requiredClue_; }

private:
    std::vector<Id> exits_;
    Id requiredClue_;
};

// A node in the clue DAG. prerequisites() are incoming edges: clues you must hold first.
// contradicts(): ID of the suspect whose alibi this clue breaks ("" = none, i.e. neutral/red herring).
class Clue : public Entity {
public:
    // A clue that is NOT lying around in a room: it is earned (a confession, a lab result, a doctor's finding).
    Clue& grantedOnly() { granted_ = true; return *this; }
    bool granted() const { return granted_; }
    Clue(Id id, std::string name, std::string desc, Id location,
         std::vector<Id> prerequisites = {}, Id contradicts = "")
        : Entity(std::move(id), std::move(name), std::move(desc)),
          location_(std::move(location)), prerequisites_(std::move(prerequisites)),
          contradicts_(std::move(contradicts)) {}
    std::string kind() const override { return "Clue"; }
    const Id& location() const { return location_; }
    const std::vector<Id>& prerequisites() const { return prerequisites_; }
    const Id& contradicts() const { return contradicts_; }

private:
    Id location_;
    std::vector<Id> prerequisites_;
    Id contradicts_;
    bool granted_ = false;
};

class Suspect : public Entity {
public:
    // A witness (e.g. the doctor) can be questioned but never accused.
    Suspect& witness() { witness_ = true; return *this; }
    bool accusable() const { return !witness_; }
    Suspect(Id id, std::string name, std::string desc, std::string alibi)
        : Entity(std::move(id), std::move(name), std::move(desc)), alibi_(std::move(alibi)) {}
    std::string kind() const override { return "Suspect"; }
    const std::string& alibi() const { return alibi_; }

private:
    std::string alibi_;
    bool witness_ = false;
};

// One thing a person claims. If it is a lie, showing the right evidence exposes it (cross-examination).
struct Statement {
    std::string text;                 // what they say
    std::string press;                // extra detail when the player presses them
    Id pressReveals;                  // clue earned by pressing ("" = none)
    std::vector<Id> contradictedBy;   // clues that prove this false (empty = it is true)
    std::string confession;           // how they react when caught
    Id reveals;                       // clue earned when caught
    bool incriminating = false;       // true only for lies that point at the killer
    bool isLie() const { return !contradictedBy.empty(); }
};

// A forensic test the player can run once they hold the right evidence.
struct LabTask {
    Id id;
    std::string title;
    std::vector<Id> needs;   // clues required
    Id grants;               // clue earned
};
